#pragma once

#include <cstdint>

// Shared-memory IPC contract between 64-bit CalculadoraUnificada.exe (host)
// and 32-bit cg50_official_gcal_host32.exe (isolated official Casio GCal DLL worker).
// Strictly fixed-width fields with 8-byte alignment so x86 and x64 match byte-for-byte.

#pragma pack(push, 8)
struct Cg50OfficialShmBlock {
    static constexpr std::uint32_t kMagic = 0x43473530u; // "CG50"
    static constexpr std::uint32_t kVersion = 1u;
    static constexpr std::uint32_t kMaxW = 384u;
    static constexpr std::uint32_t kMaxH = 216u;
    static constexpr std::uint32_t kMaxPixels = kMaxW * kMaxH;

    enum Command : std::uint32_t {
        CmdNone      = 0,
        CmdInit      = 1,
        CmdStep      = 2,
        CmdKeyDown   = 3,
        CmdKeyUp     = 4,
        CmdKeyStroke = 5,
        CmdReset     = 6,
        CmdShutdown  = 7
    };

    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t cmd;
    std::uint32_t arg1;
    std::uint32_t arg2;
    std::int32_t  cmdResult;
    std::int32_t  state;        // 0=Unavailable, 1=Ready, 2=Faulted
    std::uint32_t currentStage; // 0..11
    std::uint32_t fbWidth;
    std::uint32_t fbHeight;
    std::uint32_t fbStride;
    std::uint32_t fbValid;
    std::uint64_t totalCpuCycles;
    std::uint32_t frameCount;
    std::uint32_t reserved0;
    double        lastStepMs;
    std::uint16_t diagnosticUtf16[512];
    std::uint32_t pixels[kMaxPixels]; // 384 x 216 BGRA32 (0xFFRRGGBB)
};
#pragma pack(pop)
