#pragma once

#include "cg50_fugue_ftl.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cg50 {

struct SmemEntry {
    std::string name;       // Preserved UTF-8 name (LFN or 8.3)
    std::string shortName;  // Formatted 8.3 name (e.g. "HOLA_U~1.PY")
    std::string fullPath;   // Normalized path starting with '/'
    bool isDir = false;
    std::uint32_t size = 0;
    std::uint16_t firstCluster = 0;
    std::uint16_t modDate = 0;
    std::uint16_t modTime = 0;
    std::uint8_t attr = 0;
};

struct SmemVolumeInfo {
    std::string volumeLabel;
    std::uint32_t bytesPerSector = 512;
    std::uint32_t sectorsPerCluster = 8;
    std::uint32_t clusterSize = 4096;
    std::uint32_t totalSectors = 0;
    std::uint32_t totalClusters = 0;
    std::uint32_t freeClusters = 0;
    std::uint64_t totalBytes = 0;
    std::uint64_t freeBytes = 0;
    std::uint64_t usedBytes = 0;
    bool fatTablesEqual = false;
};

// Direct FAT16 + VFAT LFN filesystem manager over Casio fx-CG50 Fugue FTL.
class Cg50SmemFs {
public:
    Cg50SmemFs() = default;

    bool Mount(std::uint8_t* userRom, std::size_t userRomSize,
               const std::uint8_t* c2Rom, std::size_t c2RomSize);

    bool IsMounted() const noexcept { return mounted_; }
    const std::string& LastError() const noexcept { return lastError_; }
    const Cg50FugueFtl& Ftl() const noexcept { return ftl_; }

    bool GetVolumeInfo(SmemVolumeInfo& outInfo) const;
    bool ListDirectory(const std::string& path, std::vector<SmemEntry>& outEntries) const;
    bool ReadFile(const std::string& path, std::vector<std::uint8_t>& outData) const;
    bool WriteFile(const std::string& path, const std::uint8_t* data, std::size_t len,
                   bool overwrite = true);
    bool MakeDirectory(const std::string& path);
    bool DeletePath(const std::string& path, bool recursive = false);

    // Raw block device access (for virtual USB Mass Storage BOT/SCSI).
    std::uint32_t TotalLogicalSectors() const noexcept { return vbrLba_ + totalSectors_; }
    bool ReadRawSector(std::uint32_t lba, std::uint8_t out512[512]) const;
    bool WriteRawSector(std::uint32_t lba, const std::uint8_t in512[512]);
    bool FlushRawWrites();

private:
    struct DirEntryLoc {
        SmemEntry entry;
        std::uint16_t dirCluster = 0; // 0 = root dir
        std::uint32_t startSlot = 0;  // first LFN slot or 8.3 slot
        std::uint32_t slotCount = 1;  // number of 32-byte slots (LFNs + 8.3)
        std::uint32_t shortSlot = 0;  // exact 8.3 slot index
    };

    bool ParseBpb();
    bool ReadFatTable(std::uint8_t fatIndex, std::vector<std::uint16_t>& outFat) const;
    bool WriteFatTables(const std::vector<std::uint16_t>& fat);
    bool ReadDirSlots(std::uint16_t dirCluster, std::vector<std::uint8_t>& outBuf,
                      std::vector<std::uint32_t>& outSectors) const;
    bool WriteDirSlot(const std::vector<std::uint32_t>& dirSectors, std::uint32_t slotIdx,
                      const std::uint8_t entry32[32]);
    bool ParseDirectory(std::uint16_t dirCluster, const std::string& dirPath,
                        std::vector<DirEntryLoc>& outLocs) const;
    bool ResolveParentAndLeaf(const std::string& path, std::uint16_t& outParentCluster,
                              std::string& outParentPath, std::string& outLeaf) const;
    bool FindInDir(std::uint16_t dirCluster, const std::string& dirPath,
                   const std::string& name, DirEntryLoc& outLoc) const;
    bool AllocateChain(std::vector<std::uint16_t>& fat, std::size_t count,
                       std::vector<std::uint16_t>& outClusters) const;
    void FreeChain(std::vector<std::uint16_t>& fat, std::uint16_t firstCluster) const;
    bool WriteClusterData(std::uint16_t cluster, const std::uint8_t* data, std::size_t len);
    bool ReadClusterChain(const std::vector<std::uint16_t>& fat, std::uint16_t firstCluster,
                          std::vector<std::uint16_t>& outChain) const;
    bool AppendEntriesToDir(std::uint16_t dirCluster, std::vector<std::uint16_t>& fat,
                            const std::vector<std::array<std::uint8_t, 32>>& entries);
    bool DeleteDirLocRecursive(const DirEntryLoc& loc, std::vector<std::uint16_t>& fat,
                               bool recursive);

    std::uint32_t ClusterToLba(std::uint16_t cluster) const noexcept {
        return dataStartLba_ + static_cast<std::uint32_t>(cluster - 2u) * sectorsPerCluster_;
    }

    Cg50FugueFtl ftl_;
    bool mounted_ = false;
    mutable std::string lastError_;

    std::uint32_t vbrLba_ = 29;
    std::uint16_t bytesPerSector_ = 512;
    std::uint8_t sectorsPerCluster_ = 8;
    std::uint16_t reservedSectors_ = 1;
    std::uint8_t numFats_ = 2;
    std::uint16_t rootEntryCount_ = 512;
    std::uint32_t totalSectors_ = 0;
    std::uint16_t sectorsPerFat_ = 17;
    std::uint32_t fat1StartLba_ = 30;
    std::uint32_t rootDirStartLba_ = 64;
    std::uint32_t rootDirSectors_ = 32;
    std::uint32_t dataStartLba_ = 96;
    std::uint32_t totalClusters_ = 0;
    std::string volumeLabel_;
};

} // namespace cg50
