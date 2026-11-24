#include "cg50_fugue_ftl.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace cg50 {

namespace {

inline std::uint16_t ReadBe16(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

inline std::uint32_t ReadBe32(const std::uint8_t* p) noexcept
{
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

inline void WriteBe16(std::uint8_t* p, std::uint16_t v) noexcept
{
    p[0] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
    p[1] = static_cast<std::uint8_t>(v & 0xFFu);
}

inline void WriteBe32(std::uint8_t* p, std::uint32_t v) noexcept
{
    p[0] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
    p[1] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
    p[2] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
    p[3] = static_cast<std::uint8_t>(v & 0xFFu);
}

inline std::uint8_t ByteParity(std::uint8_t b) noexcept
{
    b ^= static_cast<std::uint8_t>(b >> 4);
    b ^= static_cast<std::uint8_t>(b >> 2);
    b ^= static_cast<std::uint8_t>(b >> 1);
    return static_cast<std::uint8_t>(b & 1u);
}

} // namespace

std::uint16_t Cg50FugueFtl::ComputeCrc16Ccitt(const std::uint8_t* data,
                                              const std::size_t len) noexcept
{
    std::uint16_t crc = 0xFFFFu;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[i]) << 8);
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = static_cast<std::uint16_t>((crc << 1) ^ 0x1021u);
            } else {
                crc = static_cast<std::uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

std::array<std::uint8_t, 3> Cg50FugueFtl::ComputeSectorEcc512(
    const std::uint8_t* sec512) noexcept
{
    std::uint8_t lp[18] = {};
    std::uint8_t cp[6] = {};
    for (std::size_t i = 0; i < 512u; ++i) {
        const std::uint8_t b = sec512[i];
        if (ByteParity(b) != 0u) {
            for (std::size_t bit = 0; bit < 9u; ++bit) {
                lp[2u * bit + ((i >> bit) & 1u)] ^= 1u;
            }
        }
        for (std::size_t k = 0; k < 8u; ++k) {
            if (((b >> k) & 1u) != 0u) {
                cp[0u + (k & 1u)] ^= 1u;
                cp[2u + ((k >> 1) & 1u)] ^= 1u;
                cp[4u + ((k >> 2) & 1u)] ^= 1u;
            }
        }
    }
    std::uint32_t v = 0u;
    for (int idx = 5; idx >= 0; --idx) {
        v = (v << 1) | static_cast<std::uint32_t>(cp[idx] ^ 1u);
    }
    for (int idx = 17; idx >= 0; --idx) {
        v = (v << 1) | static_cast<std::uint32_t>(lp[idx] ^ 1u);
    }
    return {
        static_cast<std::uint8_t>((v >> 16) & 0xFFu),
        static_cast<std::uint8_t>((v >> 8) & 0xFFu),
        static_cast<std::uint8_t>(v & 0xFFu),
    };
}

std::array<std::uint8_t, 3> Cg50FugueFtl::ComputeHeaderEcc34(
    const std::uint8_t* hdr34) noexcept
{
    std::uint8_t lp[16] = {};
    std::uint8_t cp[6] = {};
    for (std::size_t i = 0; i < 34u; ++i) {
        const std::uint8_t b = hdr34[i];
        if (ByteParity(b) != 0u) {
            for (std::size_t bit = 0; bit < 8u; ++bit) {
                lp[2u * bit + ((i >> bit) & 1u)] ^= 1u;
            }
        }
        for (std::size_t k = 0; k < 8u; ++k) {
            if (((b >> k) & 1u) != 0u) {
                cp[0u + (k & 1u)] ^= 1u;
                cp[2u + ((k >> 1) & 1u)] ^= 1u;
                cp[4u + ((k >> 2) & 1u)] ^= 1u;
            }
        }
    }
    std::uint32_t v = 0x3u; // top two bits = 11
    for (int idx = 5; idx >= 0; --idx) {
        v = (v << 1) | static_cast<std::uint32_t>(cp[idx] ^ 1u);
    }
    for (int idx = 7; idx >= 0; --idx) {
        v = (v << 1) | static_cast<std::uint32_t>(lp[idx] ^ 1u);
    }
    for (int idx = 15; idx >= 8; --idx) {
        v = (v << 1) | static_cast<std::uint32_t>(lp[idx] ^ 1u);
    }
    return {
        static_cast<std::uint8_t>((v >> 16) & 0xFFu),
        static_cast<std::uint8_t>((v >> 8) & 0xFFu),
        static_cast<std::uint8_t>(v & 0xFFu),
    };
}

void Cg50FugueFtl::BuildSlot0Descriptor(const std::uint8_t* page0,
                                        std::uint8_t outDesc[kDescriptorSize]) noexcept
{
    std::memset(outDesc, 0xFF, kDescriptorSize);
    outDesc[0] = 0x22u;
    outDesc[1] = 0x0Fu;
    WriteBe16(outDesc + 8, ComputeCrc16Ccitt(page0, kPageSize));
    for (std::size_t s = 0; s < kSectorsPerPage; ++s) {
        const auto ecc = ComputeSectorEcc512(page0 + s * kSectorSize);
        outDesc[10 + s * 3] = ecc[0];
        outDesc[11 + s * 3] = ecc[1];
        outDesc[12 + s * 3] = ecc[2];
    }
    outDesc[34] = 0x5Au;
    outDesc[35] = 0x5Au;
    outDesc[36] = 0x5Au;
    const auto hdrEcc = ComputeHeaderEcc34(outDesc);
    outDesc[37] = hdrEcc[0];
    outDesc[38] = hdrEcc[1];
    outDesc[39] = hdrEcc[2];
}

void Cg50FugueFtl::BuildDataDescriptor(const std::uint16_t lpn,
                                       const std::uint32_t rev,
                                       const std::uint8_t* page4096,
                                       std::uint8_t outDesc[kDescriptorSize]) noexcept
{
    std::memset(outDesc, 0xFF, kDescriptorSize);
    outDesc[0] = 0x11u;
    outDesc[1] = 0x0Fu;
    WriteBe32(outDesc + 2, ~rev);
    WriteBe16(outDesc + 6, lpn);
    WriteBe16(outDesc + 8, ComputeCrc16Ccitt(page4096, kPageSize));
    for (std::size_t s = 0; s < kSectorsPerPage; ++s) {
        const auto ecc = ComputeSectorEcc512(page4096 + s * kSectorSize);
        outDesc[10 + s * 3] = ecc[0];
        outDesc[11 + s * 3] = ecc[1];
        outDesc[12 + s * 3] = ecc[2];
    }
    outDesc[34] = 0x5Au;
    outDesc[35] = 0x5Au;
    outDesc[36] = 0x5Au;
    const auto hdrEcc = ComputeHeaderEcc34(outDesc);
    outDesc[37] = hdrEcc[0];
    outDesc[38] = hdrEcc[1];
    outDesc[39] = hdrEcc[2];
}

bool Cg50FugueFtl::IsBlockAllFF(const std::uint8_t* blk) const noexcept
{
    for (std::size_t i = 0; i < kBlockSize; ++i) {
        if (blk[i] != 0xFFu) return false;
    }
    return true;
}

bool Cg50FugueFtl::ValidateSlot0BlockHeader(const std::uint8_t* blk,
                                            std::uint32_t& outSeq) const noexcept
{
    const std::uint8_t* d0 = blk + kDescriptorTableOffset;
    if (d0[0] != 0x22u || d0[34] != 0x5Au || d0[35] != 0x5Au || d0[36] != 0x5Au) {
        return false;
    }
    // TMR triple-redundant (~seq, seq) at page 0 offset 0..23
    for (std::size_t copy = 0; copy < 3u; ++copy) {
        const std::uint32_t nseq = ReadBe32(blk + copy * 8u);
        const std::uint32_t seq = ReadBe32(blk + copy * 8u + 4u);
        if ((~nseq) == seq) {
            outSeq = seq;
            return true;
        }
    }
    return false;
}

bool Cg50FugueFtl::Mount(std::uint8_t* userRom, const std::size_t userRomSize,
                         const std::uint8_t* c2Rom, const std::size_t c2RomSize)
{
    mounted_ = false;
    lastError_.clear();
    dirtyPages_.clear();
    lpnMap_.clear();
    userBlocks_.clear();
    maxBlockSeq_ = 0;

    if (userRom == nullptr || userRomSize < kBlockSize) {
        lastError_ = "UserROM buffer missing or smaller than one 128 KB Fugue block";
        return false;
    }

    userRom_ = userRom;
    userRomSize_ = userRomSize;
    userRomFtlBlocks_ = std::min<std::size_t>(userRomSize / kBlockSize, kMaxUserRomFtlBlocks);
    c2Rom_ = (c2Rom != nullptr && c2RomSize >= kBlockSize) ? c2Rom : nullptr;
    c2RomSize_ = c2Rom_ ? c2RomSize : 0u;
    c2RomBlocks_ = c2Rom_ ? (c2RomSize_ / kBlockSize) : 0u;

    if (!ScanPool()) {
        return false;
    }
    mounted_ = true;
    return true;
}

bool Cg50FugueFtl::ScanPool()
{
    lpnMap_.clear();
    userBlocks_.assign(userRomFtlBlocks_, BlockMeta{});
    maxBlockSeq_ = 0;

    auto scanBank = [&](const std::uint8_t* base, const std::size_t nBlocks, const bool isC2) {
        for (std::size_t b = 0; b < nBlocks; ++b) {
            const std::uint8_t* blk = base + b * kBlockSize;
            std::uint32_t seq = 0;
            if (ValidateSlot0BlockHeader(blk, seq)) {
                if (seq > maxBlockSeq_) maxBlockSeq_ = seq;
                if (!isC2) {
                    userBlocks_[b].kind = BlockKind::ActiveFugue;
                    userBlocks_[b].seq = seq;
                    userBlocks_[b].firstFreeSlot = 31;
                    userBlocks_[b].freeSlotCount = 0;
                }
                for (std::size_t s = 1; s <= kDataSlotsPerBlock; ++s) {
                    const std::uint8_t* desc = blk + kDescriptorTableOffset + s * kDescriptorSize;
                    if (desc[0] == 0xFFu) {
                        if (!isC2) {
                            if (userBlocks_[b].firstFreeSlot == 31) {
                                userBlocks_[b].firstFreeSlot = static_cast<std::uint8_t>(s);
                            }
                            ++userBlocks_[b].freeSlotCount;
                        }
                        continue;
                    }
                    if (desc[0] != 0x11u || desc[34] != 0x5Au || desc[35] != 0x5Au || desc[36] != 0x5Au) {
                        continue;
                    }
                    const std::uint32_t rev = ~ReadBe32(desc + 2);
                    const std::uint16_t lpn = ReadBe16(desc + 6);
                    auto it = lpnMap_.find(lpn);
                    const bool newer = (it == lpnMap_.end()) ||
                                       (rev > it->second.rev) ||
                                       (rev == it->second.rev && seq > it->second.blockSeq);
                    if (newer) {
                        PageLocation loc;
                        loc.inC2 = isC2;
                        loc.blockIndex = static_cast<std::uint16_t>(b);
                        loc.slotIndex = static_cast<std::uint8_t>(s);
                        loc.rev = rev;
                        loc.blockSeq = seq;
                        lpnMap_[lpn] = loc;
                    }
                }
            } else if (!isC2) {
                if (IsBlockAllFF(blk)) {
                    userBlocks_[b].kind = BlockKind::Erased;
                    userBlocks_[b].seq = 0;
                    userBlocks_[b].firstFreeSlot = 1;
                    userBlocks_[b].freeSlotCount = static_cast<std::uint8_t>(kDataSlotsPerBlock);
                } else {
                    userBlocks_[b].kind = BlockKind::ReservedOrUnknown;
                }
            }
        }
    };

    if (c2Rom_ != nullptr && c2RomBlocks_ > 0) {
        scanBank(c2Rom_, c2RomBlocks_, true);
    }
    scanBank(userRom_, userRomFtlBlocks_, false);

    if (lpnMap_.empty()) {
        lastError_ = "No valid Fugue FTL pages found in UserROM/C2ROM";
        return false;
    }
    return true;
}

Cg50FugueFtl::Stats Cg50FugueFtl::GetStats() const noexcept
{
    Stats st;
    st.userRomBlocksTotal = static_cast<std::uint32_t>(userRomFtlBlocks_);
    st.mappedPages = static_cast<std::uint32_t>(lpnMap_.size());
    st.maxBlockSeq = maxBlockSeq_;
    for (const auto& bm : userBlocks_) {
        if (bm.kind == BlockKind::ActiveFugue) {
            ++st.userRomBlocksUsed;
            st.freeSlotsInActiveUserBlocks += bm.freeSlotCount;
        } else if (bm.kind == BlockKind::Erased) {
            ++st.userRomBlocksErased;
        }
    }
    if (c2Rom_ != nullptr) {
        for (std::size_t b = 0; b < c2RomBlocks_; ++b) {
            std::uint32_t seq = 0;
            if (ValidateSlot0BlockHeader(c2Rom_ + b * kBlockSize, seq)) {
                ++st.c2RomBlocksUsed;
            }
        }
    }
    return st;
}

bool Cg50FugueFtl::ReadPage(const std::uint16_t lpn, std::uint8_t out4096[kPageSize]) const
{
    if (!mounted_ || out4096 == nullptr) return false;

    auto dirtyIt = dirtyPages_.find(lpn);
    if (dirtyIt != dirtyPages_.end()) {
        std::memcpy(out4096, dirtyIt->second.data(), kPageSize);
        return true;
    }

    auto it = lpnMap_.find(lpn);
    if (it == lpnMap_.end()) {
        std::memset(out4096, 0xFF, kPageSize);
        return true;
    }

    const PageLocation& loc = it->second;
    const std::uint8_t* bank = loc.inC2 ? c2Rom_ : userRom_;
    const std::size_t bankSize = loc.inC2 ? c2RomSize_ : userRomSize_;
    const std::size_t off = static_cast<std::size_t>(loc.blockIndex) * kBlockSize +
                            static_cast<std::size_t>(loc.slotIndex) * kPageSize;
    if (bank == nullptr || off + kPageSize > bankSize) {
        return false;
    }
    std::memcpy(out4096, bank + off, kPageSize);
    return true;
}

bool Cg50FugueFtl::WritePage(const std::uint16_t lpn, const std::uint8_t in4096[kPageSize])
{
    if (!mounted_ || in4096 == nullptr) return false;
    std::array<std::uint8_t, kPageSize> copy{};
    std::memcpy(copy.data(), in4096, kPageSize);
    dirtyPages_[lpn] = copy;
    return true;
}

bool Cg50FugueFtl::ReadSector(const std::uint32_t lba,
                              std::uint8_t out512[kSectorSize]) const
{
    if (!mounted_ || out512 == nullptr) return false;
    const std::uint32_t lpn32 = lba / kSectorsPerPage;
    if (lpn32 > 0xFFFEu) return false;
    const std::uint32_t secInPage = lba % kSectorsPerPage;
    std::array<std::uint8_t, kPageSize> page{};
    if (!ReadPage(static_cast<std::uint16_t>(lpn32), page.data())) return false;
    std::memcpy(out512, page.data() + secInPage * kSectorSize, kSectorSize);
    return true;
}

bool Cg50FugueFtl::WriteSector(const std::uint32_t lba,
                               const std::uint8_t in512[kSectorSize])
{
    if (!mounted_ || in512 == nullptr) return false;
    const std::uint32_t lpn32 = lba / kSectorsPerPage;
    if (lpn32 > 0xFFFEu) {
        lastError_ = "LBA out of Fugue LPN range";
        return false;
    }
    const std::uint16_t lpn = static_cast<std::uint16_t>(lpn32);
    const std::uint32_t secInPage = lba % kSectorsPerPage;
    auto it = dirtyPages_.find(lpn);
    if (it == dirtyPages_.end()) {
        std::array<std::uint8_t, kPageSize> page{};
        if (!ReadPage(lpn, page.data())) {
            lastError_ = "Failed reading backing page for sector RMW";
            return false;
        }
        auto res = dirtyPages_.emplace(lpn, page);
        it = res.first;
    }
    std::memcpy(it->second.data() + secInPage * kSectorSize, in512, kSectorSize);
    return true;
}

void Cg50FugueFtl::DiscardDirty() noexcept
{
    dirtyPages_.clear();
}

bool Cg50FugueFtl::InitErasedUserBlock(const std::uint16_t blockIndex,
                                       const std::uint32_t seq)
{
    if (blockIndex >= userRomFtlBlocks_ || userBlocks_[blockIndex].kind != BlockKind::Erased) {
        return false;
    }
    std::uint8_t* blk = userRom_ + static_cast<std::size_t>(blockIndex) * kBlockSize;
    std::memset(blk, 0xFF, kBlockSize);
    const std::uint32_t nseq = ~seq;
    for (std::size_t copy = 0; copy < 3u; ++copy) {
        WriteBe32(blk + copy * 8u, nseq);
        WriteBe32(blk + copy * 8u + 4u, seq);
    }
    std::uint8_t desc0[kDescriptorSize];
    BuildSlot0Descriptor(blk, desc0);
    std::memcpy(blk + kDescriptorTableOffset, desc0, kDescriptorSize);

    userBlocks_[blockIndex].kind = BlockKind::ActiveFugue;
    userBlocks_[blockIndex].seq = seq;
    userBlocks_[blockIndex].firstFreeSlot = 1;
    userBlocks_[blockIndex].freeSlotCount = static_cast<std::uint8_t>(kDataSlotsPerBlock);
    if (seq > maxBlockSeq_) maxBlockSeq_ = seq;
    return true;
}

bool Cg50FugueFtl::GarbageCollectOneUserBlock()
{
    // Count live (latest-revision) pages per UserROM block.
    std::vector<std::uint8_t> liveCount(userRomFtlBlocks_, 0u);
    std::vector<std::vector<std::uint16_t>> liveLpns(userRomFtlBlocks_);
    for (const auto& kv : lpnMap_) {
        if (!kv.second.inC2 && kv.second.blockIndex < userRomFtlBlocks_) {
            ++liveCount[kv.second.blockIndex];
            liveLpns[kv.second.blockIndex].push_back(kv.first);
        }
    }

    // Choose an ActiveFugue UserROM block with strictly fewer than 30 live pages
    // (preferring the lowest live count to reclaim maximum space with minimal copies).
    std::size_t victim = userRomFtlBlocks_;
    std::uint8_t bestLive = static_cast<std::uint8_t>(kDataSlotsPerBlock);
    for (std::size_t b = 0; b < userRomFtlBlocks_; ++b) {
        if (userBlocks_[b].kind != BlockKind::ActiveFugue) continue;
        if (userBlocks_[b].freeSlotCount > 0) continue; // don't GC a partially filled block
        if (liveCount[b] < bestLive) {
            bestLive = liveCount[b];
            victim = b;
        }
    }

    if (victim >= userRomFtlBlocks_) {
        lastError_ = "Fugue Flash full: no reclaimable UserROM block found";
        return false;
    }

    // Copy out the victim's live pages into memory first so we can safely erase the victim
    // even if 0 erased blocks currently remain.
    struct SavedPage {
        std::uint16_t lpn;
        std::uint32_t nextRev;
        std::array<std::uint8_t, kPageSize> data;
    };
    std::vector<SavedPage> survivors;
    survivors.reserve(liveLpns[victim].size());
    for (const std::uint16_t lpn : liveLpns[victim]) {
        const PageLocation& loc = lpnMap_.at(lpn);
        SavedPage sp;
        sp.lpn = lpn;
        sp.nextRev = loc.rev + 1u;
        const std::uint8_t* src = userRom_ +
            static_cast<std::size_t>(loc.blockIndex) * kBlockSize +
            static_cast<std::size_t>(loc.slotIndex) * kPageSize;
        std::memcpy(sp.data.data(), src, kPageSize);
        survivors.push_back(sp);
    }

    // Erase victim block to 0xFF.
    std::uint8_t* victimPtr = userRom_ + victim * kBlockSize;
    std::memset(victimPtr, 0xFF, kBlockSize);
    userBlocks_[victim].kind = BlockKind::Erased;
    userBlocks_[victim].seq = 0;
    userBlocks_[victim].firstFreeSlot = 1;
    userBlocks_[victim].freeSlotCount = static_cast<std::uint8_t>(kDataSlotsPerBlock);

    if (survivors.empty()) {
        return true;
    }

    // Re-initialize victim with next sequence number and write back only its live pages,
    // leaving (30 - survivors.size()) clean free slots in the block.
    if (!InitErasedUserBlock(static_cast<std::uint16_t>(victim), maxBlockSeq_ + 1u)) {
        lastError_ = "Failed to re-initialize reclaimed Fugue block";
        return false;
    }
    const std::uint32_t newSeq = userBlocks_[victim].seq;
    for (std::size_t i = 0; i < survivors.size(); ++i) {
        const std::uint8_t slot = static_cast<std::uint8_t>(i + 1u);
        std::memcpy(victimPtr + static_cast<std::size_t>(slot) * kPageSize,
                    survivors[i].data.data(), kPageSize);
        std::uint8_t desc[kDescriptorSize];
        BuildDataDescriptor(survivors[i].lpn, survivors[i].nextRev,
                            survivors[i].data.data(), desc);
        std::memcpy(victimPtr + kDescriptorTableOffset +
                        static_cast<std::size_t>(slot) * kDescriptorSize,
                    desc, kDescriptorSize);

        PageLocation loc;
        loc.inC2 = false;
        loc.blockIndex = static_cast<std::uint16_t>(victim);
        loc.slotIndex = slot;
        loc.rev = survivors[i].nextRev;
        loc.blockSeq = newSeq;
        lpnMap_[survivors[i].lpn] = loc;
    }
    userBlocks_[victim].firstFreeSlot = static_cast<std::uint8_t>(survivors.size() + 1u);
    userBlocks_[victim].freeSlotCount =
        static_cast<std::uint8_t>(kDataSlotsPerBlock - survivors.size());
    return true;
}

bool Cg50FugueFtl::AllocateSlot(std::uint16_t& outBlock, std::uint8_t& outSlot)
{
    // 1. Prefer an already-active UserROM block with free slots (highest seq first).
    std::size_t bestActive = userRomFtlBlocks_;
    std::uint32_t bestSeq = 0;
    for (std::size_t b = 0; b < userRomFtlBlocks_; ++b) {
        if (userBlocks_[b].kind == BlockKind::ActiveFugue &&
            userBlocks_[b].freeSlotCount > 0 &&
            userBlocks_[b].firstFreeSlot <= kDataSlotsPerBlock) {
            if (bestActive == userRomFtlBlocks_ || userBlocks_[b].seq >= bestSeq) {
                bestActive = b;
                bestSeq = userBlocks_[b].seq;
            }
        }
    }
    if (bestActive < userRomFtlBlocks_) {
        outBlock = static_cast<std::uint16_t>(bestActive);
        outSlot = userBlocks_[bestActive].firstFreeSlot;
        return true;
    }

    // 2. Otherwise initialize an erased 0xFF UserROM block (keep normal wear distribution by picking first erased).
    for (std::size_t b = 0; b < userRomFtlBlocks_; ++b) {
        if (userBlocks_[b].kind == BlockKind::Erased) {
            if (!InitErasedUserBlock(static_cast<std::uint16_t>(b), maxBlockSeq_ + 1u)) {
                return false;
            }
            outBlock = static_cast<std::uint16_t>(b);
            outSlot = 1;
            return true;
        }
    }

    // 3. No free slots and no erased blocks -> garbage collect a block with obsolete pages.
    if (!GarbageCollectOneUserBlock()) {
        return false;
    }
    return AllocateSlot(outBlock, outSlot);
}

bool Cg50FugueFtl::WritePhysicalPage(const std::uint16_t lpn,
                                     const std::uint8_t page[kPageSize])
{
    std::uint16_t blockIdx = 0;
    std::uint8_t slotIdx = 0;
    if (!AllocateSlot(blockIdx, slotIdx)) {
        return false;
    }

    std::uint32_t nextRev = 1u;
    auto it = lpnMap_.find(lpn);
    if (it != lpnMap_.end()) {
        nextRev = it->second.rev + 1u;
    }

    std::uint8_t* blk = userRom_ + static_cast<std::size_t>(blockIdx) * kBlockSize;
    std::memcpy(blk + static_cast<std::size_t>(slotIdx) * kPageSize, page, kPageSize);

    std::uint8_t desc[kDescriptorSize];
    BuildDataDescriptor(lpn, nextRev, page, desc);
    std::memcpy(blk + kDescriptorTableOffset +
                    static_cast<std::size_t>(slotIdx) * kDescriptorSize,
                desc, kDescriptorSize);

    // Advance block free-slot cursor.
    auto& bm = userBlocks_[blockIdx];
    if (bm.freeSlotCount > 0) --bm.freeSlotCount;
    bm.firstFreeSlot = 31;
    for (std::size_t s = static_cast<std::size_t>(slotIdx) + 1u; s <= kDataSlotsPerBlock; ++s) {
        if (blk[kDescriptorTableOffset + s * kDescriptorSize] == 0xFFu) {
            bm.firstFreeSlot = static_cast<std::uint8_t>(s);
            break;
        }
    }

    PageLocation loc;
    loc.inC2 = false;
    loc.blockIndex = blockIdx;
    loc.slotIndex = slotIdx;
    loc.rev = nextRev;
    loc.blockSeq = bm.seq;
    lpnMap_[lpn] = loc;
    return true;
}

bool Cg50FugueFtl::Flush()
{
    if (!mounted_) {
        lastError_ = "Fugue FTL not mounted";
        return false;
    }
    if (dirtyPages_.empty()) return true;

    // Take transactional backup of the UserROM FTL region so any out-of-space
    // failure leaves UserROM completely untouched.
    const std::size_t backupBytes = userRomFtlBlocks_ * kBlockSize;
    std::vector<std::uint8_t> backup(userRom_, userRom_ + backupBytes);

    // Deterministic write order by LPN.
    std::vector<std::uint16_t> lpns;
    lpns.reserve(dirtyPages_.size());
    for (const auto& kv : dirtyPages_) {
        lpns.push_back(kv.first);
    }
    std::sort(lpns.begin(), lpns.end());

    for (const std::uint16_t lpn : lpns) {
        if (!WritePhysicalPage(lpn, dirtyPages_.at(lpn).data())) {
            std::memcpy(userRom_, backup.data(), backupBytes);
            ScanPool();
            dirtyPages_.clear();
            return false;
        }
    }

    dirtyPages_.clear();
    return true;
}

} // namespace cg50
