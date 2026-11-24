#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace cg50 {

// Bit-exact Casio fx-CG50 Fugue NOR-Flash Translation Layer (FTL).
// Verified 99/99 blocks and 5/5 official .g3a files bit-for-bit against
// Casio UserROM.raw (128 active 128 KB blocks) + C2ROM.raw (37 128 KB blocks).
class Cg50FugueFtl {
public:
    static constexpr std::size_t kBlockSize = 0x20000u;         // 128 KB
    static constexpr std::size_t kPageSize = 0x1000u;           // 4 KB (8 sectors)
    static constexpr std::size_t kSectorSize = 512u;
    static constexpr std::size_t kSectorsPerPage = 8u;
    static constexpr std::size_t kDescriptorTableOffset = 0x1F000u;
    static constexpr std::size_t kDescriptorSize = 64u;
    static constexpr std::size_t kSlotsPerBlock = 31u;          // slot 0 header + slots 1..30 data
    static constexpr std::size_t kDataSlotsPerBlock = 30u;      // slots 1..30
    static constexpr std::size_t kMaxUserRomFtlBlocks = 128u;   // blocks 0..127 (16 MB); 128..164 are 0x00 padding

    struct PageLocation {
        bool inC2 = false;
        std::uint16_t blockIndex = 0;
        std::uint8_t slotIndex = 0;   // 1..30
        std::uint32_t rev = 0;
        std::uint32_t blockSeq = 0;
    };

    struct Stats {
        std::uint32_t userRomBlocksTotal = 0;
        std::uint32_t userRomBlocksUsed = 0;
        std::uint32_t userRomBlocksErased = 0;
        std::uint32_t c2RomBlocksUsed = 0;
        std::uint32_t mappedPages = 0;
        std::uint32_t freeSlotsInActiveUserBlocks = 0;
        std::uint32_t maxBlockSeq = 0;
    };

    Cg50FugueFtl() = default;

    // Bind backing storage buffers. userRom must be non-null (writable);
    // c2Rom may be null/empty (treated as read-only if provided).
    bool Mount(std::uint8_t* userRom, std::size_t userRomSize,
               const std::uint8_t* c2Rom, std::size_t c2RomSize);

    bool IsMounted() const noexcept { return mounted_; }
    Stats GetStats() const noexcept;
    const std::string& LastError() const noexcept { return lastError_; }

    // Sector-level (512 B) logical I/O over the Fugue volume.
    bool ReadSector(std::uint32_t lba, std::uint8_t out512[kSectorSize]) const;
    bool WriteSector(std::uint32_t lba, const std::uint8_t in512[kSectorSize]);

    // Page-level (4096 B) logical I/O.
    bool ReadPage(std::uint16_t lpn, std::uint8_t out4096[kPageSize]) const;
    bool WritePage(std::uint16_t lpn, const std::uint8_t in4096[kPageSize]);

    // Flush all staged dirty pages atomically into UserROM Fugue blocks.
    // Rolls back UserROM bytes if any allocation fails.
    bool Flush();
    void DiscardDirty() noexcept;
    bool HasDirtyPages() const noexcept { return !dirtyPages_.empty(); }

    // Exact Fugue checksum / ECC helpers (exposed for unit tests).
    static std::uint16_t ComputeCrc16Ccitt(const std::uint8_t* data, std::size_t len) noexcept;
    static std::array<std::uint8_t, 3> ComputeSectorEcc512(const std::uint8_t* sec512) noexcept;
    static std::array<std::uint8_t, 3> ComputeHeaderEcc34(const std::uint8_t* hdr34) noexcept;
    static void BuildSlot0Descriptor(const std::uint8_t* page0, std::uint8_t outDesc[kDescriptorSize]) noexcept;
    static void BuildDataDescriptor(std::uint16_t lpn, std::uint32_t rev,
                                    const std::uint8_t* page4096,
                                    std::uint8_t outDesc[kDescriptorSize]) noexcept;

private:
    enum class BlockKind : std::uint8_t {
        ReservedOrUnknown = 0,
        Erased,
        ActiveFugue,
    };

    struct BlockMeta {
        BlockKind kind = BlockKind::ReservedOrUnknown;
        std::uint32_t seq = 0;
        std::uint8_t firstFreeSlot = 31; // 1..30 if free slot exists, 31 if full
        std::uint8_t freeSlotCount = 0;
    };

    bool ScanPool();
    bool AllocateSlot(std::uint16_t& outBlock, std::uint8_t& outSlot);
    bool InitErasedUserBlock(std::uint16_t blockIndex, std::uint32_t seq);
    bool GarbageCollectOneUserBlock();
    bool WritePhysicalPage(std::uint16_t lpn, const std::uint8_t page[kPageSize]);
    bool IsBlockAllFF(const std::uint8_t* blk) const noexcept;
    bool ValidateSlot0BlockHeader(const std::uint8_t* blk, std::uint32_t& outSeq) const noexcept;

    std::uint8_t* userRom_ = nullptr;
    std::size_t userRomSize_ = 0;
    std::size_t userRomFtlBlocks_ = 0;

    const std::uint8_t* c2Rom_ = nullptr;
    std::size_t c2RomSize_ = 0;
    std::size_t c2RomBlocks_ = 0;

    bool mounted_ = false;
    std::uint32_t maxBlockSeq_ = 0;
    std::vector<BlockMeta> userBlocks_;
    std::unordered_map<std::uint16_t, PageLocation> lpnMap_;
    std::unordered_map<std::uint16_t, std::array<std::uint8_t, kPageSize>> dirtyPages_;
    std::string lastError_;
};

} // namespace cg50
