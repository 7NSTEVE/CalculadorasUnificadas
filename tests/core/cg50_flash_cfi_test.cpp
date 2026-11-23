// cg50_flash_cfi_test.cpp — Unit tests for Spansion S29GL256P NOR Flash CFI
// state machine, UserROM replacement, and .g3a header validation.

#include "cg50_native_core.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_fails = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::fprintf(stderr, "[CFI-TEST] FAIL (%s:%d): %s\n",             \
                         __FILE__, __LINE__, (msg));                          \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

void WriteCmd(Cg50NativeCore& core, std::uint32_t addr, std::uint16_t val)
{
    CHECK(core.Write16(addr, val), "Write16 CFI command");
}

void TestSectorEraseAndProtection(Cg50NativeCore& core)
{
    // Seed UserROM sector 0 (0x01000000..0x0101FFFF) and sector 1 (0x01020000)
    // directly with known bytes, then erase sector 0 via Spansion 6-cycle CFI.
    CHECK(core.Write16(0xA1000000u, 0x1122u), "seed sec0 start");
    CHECK(core.Write16(0xA101FFFEu, 0x3344u), "seed sec0 end");
    CHECK(core.Write16(0xA1020000u, 0x5566u), "seed sec1 start");

    WriteCmd(core, 0xA0000AAAu, 0x00AAu);
    WriteCmd(core, 0xA0000554u, 0x0055u);
    WriteCmd(core, 0xA0000AAAu, 0x0080u);
    WriteCmd(core, 0xA0000AAAu, 0x00AAu);
    WriteCmd(core, 0xA0000554u, 0x0055u);
    WriteCmd(core, 0xA1000000u, 0x0030u);

    std::uint16_t v0 = 0, v1 = 0, v2 = 0;
    CHECK(core.Read16(0xA1000000u, v0) && v0 == 0xFFFFu, "sec0 start erased to 0xFFFF");
    CHECK(core.Read16(0xA101FFFEu, v1) && v1 == 0xFFFFu, "sec0 end erased to 0xFFFF");
    CHECK(core.Read16(0xA1020000u, v2) && v2 == 0x5566u, "sec1 untouched by sec0 erase");
    CHECK(core.FlashCfiStats().sectorErases >= 1u, "sectorErase counter incremented");

    // Negative test: attempt sector erase on SystemROM < 0x00B20000 must NOT erase OS code
    std::uint16_t osBefore = 0, osAfter = 0;
    CHECK(core.Read16(0xA0000000u, osBefore), "read SystemROM before");
    WriteCmd(core, 0xA0000AAAu, 0x00AAu);
    WriteCmd(core, 0xA0000554u, 0x0055u);
    WriteCmd(core, 0xA0000AAAu, 0x0080u);
    WriteCmd(core, 0xA0000AAAu, 0x00AAu);
    WriteCmd(core, 0xA0000554u, 0x0055u);
    WriteCmd(core, 0xA0000000u, 0x0030u);
    CHECK(core.Read16(0xA0000000u, osAfter) && osAfter == osBefore,
          "SystemROM < 0x00B20000 protected from erase");
}

void TestWriteToBufferAndNorSemantics(Cg50NativeCore& core)
{
    // Sector 0 is erased (0xFFFF). Run exact Casio ILRAM Write-to-Buffer sequence:
    // AA@AAA -> 55@554 -> 25@target -> (N-1)@SA -> N words -> 29@SA
    constexpr std::uint32_t kSa = 0xA1000000u;
    constexpr std::uint32_t kDst = 0xA1000100u;
    const std::uint16_t payload[4] = {0x1234u, 0x5678u, 0x9ABCu, 0x0000u};

    WriteCmd(core, 0xA0000AAAu, 0x00AAu);
    WriteCmd(core, 0xA0000554u, 0x0055u);
    WriteCmd(core, kDst, 0x0025u);
    WriteCmd(core, kSa, 3u); // N - 1 = 3 (4 words)
    for (std::uint32_t i = 0; i < 4u; ++i) {
        WriteCmd(core, kDst + i * 2u, payload[i]);
    }
    WriteCmd(core, kSa, 0x0029u); // Confirm!

    // Sector header at SA must remain 0xFFFF (never overwritten with 0x0025/3/0x0029!)
    std::uint16_t hdr = 0;
    CHECK(core.Read16(kSa, hdr) && hdr == 0xFFFFu, "SA header untouched by 0x25/count/0x29");

    // All 4 programmed words must read back identically (passing DQ7 polling)
    for (std::uint32_t i = 0; i < 4u; ++i) {
        std::uint16_t got = 0;
        CHECK(core.Read16(kDst + i * 2u, got) && got == payload[i],
              "Write-to-Buffer word matches payload");
    }
    CHECK(core.FlashCfiStats().bufferPrograms >= 1u, "bufferPrograms counter incremented");

    // NOR AND-programming semantics: programming 0xF0F0 over 0x1234 yields 0x1030 (1->0 only)
    WriteCmd(core, 0xA0000AAAu, 0x00AAu);
    WriteCmd(core, 0xA0000554u, 0x0055u);
    WriteCmd(core, 0xA0000AAAu, 0x00A0u);
    WriteCmd(core, kDst, 0xF0F0u);
    std::uint16_t anded = 0;
    CHECK(core.Read16(kDst, anded) && anded == (0x1234u & 0xF0F0u),
          "Word Program enforces NOR 1->0 AND semantics");
    CHECK(core.FlashCfiStats().wordPrograms >= 1u, "wordPrograms counter incremented");
}

void TestAutoselectCfiQueryAndReset(Cg50NativeCore& core)
{
    // Autoselect (AA@AAA, 55@554, 90@AAA) -> Spansion S29GL256P IDs
    WriteCmd(core, 0xA0000AAAu, 0x00AAu);
    WriteCmd(core, 0xA0000554u, 0x0055u);
    WriteCmd(core, 0xA0000AAAu, 0x0090u);
    std::uint16_t mfg = 0, dev1 = 0, dev2 = 0, dev3 = 0;
    CHECK(core.Read16(0xA0000000u, mfg) && mfg == 0x0001u, "Autoselect Mfg ID 0x0001");
    CHECK(core.Read16(0xA0000002u, dev1) && dev1 == 0x227Eu, "Autoselect Dev ID1 0x227E");
    CHECK(core.Read16(0xA000001Cu, dev2) && dev2 == 0x2222u, "Autoselect Dev ID2 0x2222");
    CHECK(core.Read16(0xA000001Eu, dev3) && dev3 == 0x2201u, "Autoselect Dev ID3 0x2201");

    // Reset back to ReadArray
    WriteCmd(core, 0xA0000000u, 0x00F0u);
    std::uint16_t arr = 0;
    CHECK(core.Read16(0xA0000000u, arr) && arr == 0xA55Au, "ReadArray restored after 0xF0");

    // CFI Query (98@0xAA) -> "Q", "R", "Y" + 32 MB / 256 sectors / 128 KB
    WriteCmd(core, 0xA00000AAu, 0x0098u);
    std::uint16_t q = 0, r = 0, y = 0, devSz = 0, secCnt = 0, secSzHi = 0;
    CHECK(core.Read16(0xA0000020u, q) && q == 'Q', "CFI Q");
    CHECK(core.Read16(0xA0000022u, r) && r == 'R', "CFI R");
    CHECK(core.Read16(0xA0000024u, y) && y == 'Y', "CFI Y");
    CHECK(core.Read16(0xA000004Eu, devSz) && devSz == 0x0019u, "CFI size 32MB (2^25)");
    CHECK(core.Read16(0xA000005Au, secCnt) && secCnt == 0x00FFu, "CFI 256 sectors");
    CHECK(core.Read16(0xA0000060u, secSzHi) && secSzHi == 0x0002u, "CFI 128KB sector");

    // Write-to-Buffer Abort Reset sequence (AA@AAA, 55@554, F0@AAA)
    WriteCmd(core, 0xA0000AAAu, 0x00AAu);
    WriteCmd(core, 0xA0000554u, 0x0055u);
    WriteCmd(core, 0xA0000AAAu, 0x00F0u);
    CHECK(core.Read16(0xA0000000u, arr) && arr == 0xA55Au, "ReadArray restored after AA/55/F0");
}

void TestG3aAndUserRomValidation(Cg50NativeCore& core)
{
    // Negative cases for .g3a
    std::vector<std::uint8_t> badShort(0x7000u, 0x00u);
    CHECK(!Cg50NativeCore::ValidateG3aHeader(badShort.data(), badShort.size()),
          "reject .g3a without payload");
    std::vector<std::uint8_t> badMagic(0x7040u, 0x00u);
    CHECK(!Cg50NativeCore::ValidateG3aHeader(badMagic.data(), badMagic.size()),
          "reject .g3a with invalid magic");
    CHECK(!core.LoadG3aAddin(badMagic.data(), badMagic.size()),
          "LoadG3aAddin fail-closed on bad magic");

    // Positive case for .g3a with real Casio inverted "USBPower" header
    std::vector<std::uint8_t> goodG3a(0x7040u, 0xFFu);
    const char* magic = "USBPower";
    for (int i = 0; i < 8; ++i) {
        goodG3a[i] = static_cast<std::uint8_t>(static_cast<std::uint8_t>(magic[i]) ^ 0xFFu);
    }
    goodG3a[0x7000u] = 0xDEu;
    goodG3a[0x7001u] = 0xADu;
    goodG3a[0x7002u] = 0xBEu;
    goodG3a[0x7003u] = 0xEFu;
    CHECK(Cg50NativeCore::ValidateG3aHeader(goodG3a.data(), goodG3a.size()),
          "accept valid Casio .g3a header");
    CHECK(core.LoadG3aAddin(goodG3a.data(), goodG3a.size()),
          "LoadG3aAddin stages payload");
    CHECK(core.AddinStagingActive(), "staging active");
    std::uint32_t stagedWord = 0;
    CHECK(core.Read32(0x88080000u, stagedWord) && stagedWord == 0xDEADBEEFu,
          "staged .g3a payload readable at 0x08080000");

    // Negative cases for ReplaceUserRom
    std::vector<std::uint8_t> badUser(1024u, 0xFFu);
    CHECK(!core.ReplaceUserRom(badUser.data(), badUser.size()),
          "ReplaceUserRom rejects wrong size");
    std::vector<std::uint8_t> dlsim(Cg50NativeCore::kUserRomSize, 0xFFu);
    std::memcpy(dlsim.data(), "DLSimMEM", 8);
    CHECK(!core.ReplaceUserRom(dlsim.data(), dlsim.size()),
          "ReplaceUserRom rejects DLSimMEM container");

    // Positive case for ReplaceUserRom
    std::vector<std::uint8_t> freshUser(Cg50NativeCore::kUserRomSize, 0xFFu);
    freshUser[0x400u] = 0xCAu;
    freshUser[0x401u] = 0xFEu;
    CHECK(core.ReplaceUserRom(freshUser.data(), freshUser.size()),
          "ReplaceUserRom accepts valid 0x14A0000 image");
    std::uint16_t checkWord = 0;
    CHECK(core.Read16(0xA1000400u, checkWord) && checkWord == 0xCAFEu,
          "replaced UserROM immediately visible");
}

} // namespace

int main()
{
    std::vector<std::uint8_t> sys(Cg50NativeCore::kRomSize, 0xFFu);
    sys[0] = 0xA5u;
    sys[1] = 0x5Au;
    std::vector<std::uint8_t> usr(Cg50NativeCore::kUserRomSize, 0xFFu);

    Cg50NativeCore core;
    CHECK(core.LoadFirmwareParts(sys.data(), sys.size(), usr.data(), usr.size()),
          "LoadFirmwareParts synthetic");

    TestSectorEraseAndProtection(core);
    TestWriteToBufferAndNorSemantics(core);
    TestAutoselectCfiQueryAndReset(core);
    TestG3aAndUserRomValidation(core);

    std::printf("[CFI-TEST] finished failures=%d\n", g_fails);
    return g_fails == 0 ? 0 : 1;
}
