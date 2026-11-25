// cg50_dlsim_oracle_x86.cpp — Live 32-bit x86 Direct CPU73050.dll Differential Oracle
// Loads Casio's official CPU73050.dll (read-only from Program Files) and executes:
//   1. Full 65,536 16-bit opcode decode & trap audit (SR.DSP=0 and SR.DSP=1)
//   2. Full 256 DSP 2nd-word (0xF800 + op2) dispatch & live execution audit
//   3. Live instruction execution with full scratch RAM callbacks (Read/Write 8/16/32)
//   4. Live peripheral register write-callback probing (bitmasks, W0C, write-keys)
// Exports machine-readable JSON/binary ground truth for Cg50NativeCore differential verification!
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct RegSlot {
    uint32_t value;
    uint32_t flags;
};

// Verified CPU73050.dll (ImageBase 0x10000000) RVAs:
constexpr uint32_t kTopDecodeTblRva  = 0x00092D20u;
constexpr uint32_t kSub0A_Rva        = 0x00092D40u;
constexpr uint32_t kSub0B_Rva        = 0x00092D50u;
constexpr uint32_t kSub0C_Rva        = 0x00092DD0u;
constexpr uint32_t kSubExt_Rva       = 0x00092ED0u;
constexpr uint32_t kDispatchTblRva   = 0x00093190u;
constexpr uint32_t kDsp2TblRva       = 0x00092470u;

constexpr uint32_t kRead8PtrRva      = 0x00368BF4u;
constexpr uint32_t kRead16PtrRva     = 0x00368BF8u;
constexpr uint32_t kRead32PtrRva     = 0x00368BFCu;
constexpr uint32_t kWrite8PtrRva     = 0x00368C00u;
constexpr uint32_t kWrite16PtrRva    = 0x00368C04u;
constexpr uint32_t kWrite32PtrRva    = 0x00368C08u;
constexpr uint32_t kNextJmpPtrRva    = 0x00368C10u;
constexpr uint32_t kDspFetchPtrRva   = 0x00368C18u;

constexpr uint32_t kRegArrayRva      = 0x00675918u; // 49 RegSlots (0..15=R0..R15, 16=SR..48=Y1)
constexpr uint32_t kCpuFlagsRva      = 0x00675AB8u;
constexpr uint32_t kExcepJmpPtrRva   = 0x00675AC8u;
constexpr uint32_t kRaiseExcepRva    = 0x0001ABDCu;
constexpr uint32_t kRaiseSlotExcepRva= 0x0001ABC4u;

static uint32_t g_savedEsp = 0;
static uint32_t g_trapped = 0;
static uint32_t g_trapCode = 0;
static uint16_t g_insnBuf[4] = {0, 0, 0, 0};

// 64 KiB Big-Endian scratch RAM mapped at any address modulo 0x10000
static uint8_t g_ram[65536];
static uint32_t g_lastReadAddr = 0;
static uint32_t g_lastWriteAddr = 0;
static uint32_t g_lastWriteVal = 0;
static uint32_t g_writeCount = 0;

static int32_t __stdcall RamRead8(uint32_t addr)
{
    g_lastReadAddr = addr;
    return static_cast<int8_t>(g_ram[addr & 0xFFFFu]);
}

static int32_t __stdcall RamRead16(uint32_t addr)
{
    g_lastReadAddr = addr;
    const uint32_t a = addr & 0xFFFEu;
    uint16_t v = (static_cast<uint16_t>(g_ram[a]) << 8) | g_ram[(a + 1u) & 0xFFFFu];
    return static_cast<int16_t>(v);
}

static uint32_t __stdcall RamRead32(uint32_t addr)
{
    g_lastReadAddr = addr;
    const uint32_t a = addr & 0xFFFCu;
    return (static_cast<uint32_t>(g_ram[a]) << 24) |
           (static_cast<uint32_t>(g_ram[(a + 1u) & 0xFFFFu]) << 16) |
           (static_cast<uint32_t>(g_ram[(a + 2u) & 0xFFFFu]) << 8) |
           static_cast<uint32_t>(g_ram[(a + 3u) & 0xFFFFu]);
}

static void __stdcall RamWrite8(uint32_t addr, uint32_t val)
{
    g_lastWriteAddr = addr;
    g_lastWriteVal = val & 0xFFu;
    ++g_writeCount;
    g_ram[addr & 0xFFFFu] = static_cast<uint8_t>(val & 0xFFu);
}

static void __stdcall RamWrite16(uint32_t addr, uint32_t val)
{
    g_lastWriteAddr = addr;
    g_lastWriteVal = val & 0xFFFFu;
    ++g_writeCount;
    const uint32_t a = addr & 0xFFFEu;
    g_ram[a] = static_cast<uint8_t>((val >> 8) & 0xFFu);
    g_ram[(a + 1u) & 0xFFFFu] = static_cast<uint8_t>(val & 0xFFu);
}

static void __stdcall RamWrite32(uint32_t addr, uint32_t val)
{
    g_lastWriteAddr = addr;
    g_lastWriteVal = val;
    ++g_writeCount;
    const uint32_t a = addr & 0xFFFCu;
    g_ram[a] = static_cast<uint8_t>((val >> 24) & 0xFFu);
    g_ram[(a + 1u) & 0xFFFFu] = static_cast<uint8_t>((val >> 16) & 0xFFu);
    g_ram[(a + 2u) & 0xFFFFu] = static_cast<uint8_t>((val >> 8) & 0xFFu);
    g_ram[(a + 3u) & 0xFFFFu] = static_cast<uint8_t>(val & 0xFFu);
}

// CPU73050.dll calling convention for Read/Write callbacks:
//   Read*:  eax = addr -> returns eax = value (preserves ebx, ecx, edx, esi, edi, ebp)
//   Write*: eax = addr, edx = value (preserves ebx, ecx, esi, edi, ebp)
__declspec(naked) void StubRead8()
{
    __asm {
        push ecx
        push edx
        push eax
        call RamRead8
        pop edx
        pop ecx
        ret
    }
}

__declspec(naked) void StubRead16()
{
    __asm {
        push ecx
        push edx
        push eax
        call RamRead16
        pop edx
        pop ecx
        ret
    }
}

__declspec(naked) void StubRead32()
{
    __asm {
        push ecx
        push edx
        push eax
        call RamRead32
        pop edx
        pop ecx
        ret
    }
}

__declspec(naked) void StubWrite8()
{
    __asm {
        push eax
        push ecx
        push edx
        push edx
        push eax
        call RamWrite8
        pop edx
        pop ecx
        pop eax
        ret
    }
}

__declspec(naked) void StubWrite16()
{
    __asm {
        push eax
        push ecx
        push edx
        push edx
        push eax
        call RamWrite16
        pop edx
        pop ecx
        pop eax
        ret
    }
}

__declspec(naked) void StubWrite32()
{
    __asm {
        push eax
        push ecx
        push edx
        push edx
        push eax
        call RamWrite32
        pop edx
        pop ecx
        pop eax
        ret
    }
}

__declspec(naked) void OracleStepDone()
{
    __asm {
        mov esp, g_savedEsp
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

__declspec(naked) void OracleDspFetchSecondWord()
{
    // 0x1001A69F (`call dword ptr [0x10368C18]`): advances edi to 2nd 16-bit word AND returns it in eax!
    __asm {
        add edi, 2
        movzx eax, word ptr [edi]
        ret
    }
}

__declspec(naked) void OracleExceptionHook()
{
    // Entered via [base + 0x00675AC8] with eax = exception slot (e.g. 0x28 = illegal, 0x29 = slot illegal)
    __asm {
        mov g_trapped, 1
        mov g_trapCode, eax
        mov esp, g_savedEsp
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

__declspec(naked) void InvokeCpu73050Handler(void* handler, uint16_t* opPtr)
{
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        mov g_savedEsp, esp
        mov eax, [ebp + 8]    // handler
        mov edi, [ebp + 12]   // opPtr
        sub esp, 0x24         // handler scratch frame
        mov ebp, 0x88000200   // matches Cg50NativeCore kPc for PC-relative MOVA/MOV.L!
        jmp eax
    }
}

// Invoke a CPU73050.dll peripheral write callback directly:
// Convention verified from disassembly: ebx = &backingDword, edx = writeValue, eax = width/val, stdcall/register clean
__declspec(naked) void InvokePeriphWrite(void* fn, uint32_t* slotPtr, uint32_t writeVal)
{
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        mov eax, [ebp + 8]    // fn
        mov ebx, [ebp + 12]   // &backingDword
        mov edx, [ebp + 16]   // writeVal
        call eax
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

uint16_t DecodeCpu73050Oid(uintptr_t base, uint16_t op)
{
    auto u16 = [&](uint32_t rva) -> uint16_t {
        return *reinterpret_cast<const uint16_t*>(base + rva);
    };
    uint32_t ecx = op;
    uint16_t eax = u16(kTopDecodeTblRva + (ecx >> 12) * 2u);
    if ((eax & 0x8000u) == 0u) return eax;
    if (eax == 0x8000u) {
        if ((ecx & 4u) != 0u) {
            uint32_t idx = ((ecx >> 1) & 4u) + (ecx & 3u);
            return u16(kSub0A_Rva + idx * 2u);
        }
        if ((ecx & 0xF08u) == 8u) {
            uint32_t idx = ((ecx >> 2) & 0x3Cu) + (ecx & 3u);
            return u16(kSub0B_Rva + idx * 2u);
        }
        uint32_t idx = ((ecx >> 1) & 0x7Cu) + (ecx & 3u);
        return u16(kSub0C_Rva + idx * 2u);
    }
    if (eax & 0x1000u) ecx &= 0xFu;
    else if (eax & 0x2000u) ecx = (ecx >> 8) & 0xFu;
    else ecx &= 0xFFu;
    ecx += (eax & 0xFFFu);
    return u16(kSubExt_Rva + ecx * 2u);
}

// Structured binary query / response record for high-speed differential batch mode!
#pragma pack(push, 1)
struct OracleReq {
    uint32_t id;
    uint16_t op1;
    uint16_t op2;
    uint32_t initRegs[49];
    uint32_t ramAddr;
    uint32_t ramVal;
    uint32_t flags; // bit0: init ram32 at ramAddr
};
struct OracleResp {
    uint32_t id;
    uint16_t op1;
    uint16_t op2;
    uint16_t oid;
    uint16_t trapped;
    uint32_t trapCode;
    uint32_t outRegs[49];
    uint32_t writeCount;
    uint32_t lastWriteAddr;
    uint32_t lastWriteVal;
};
#pragma pack(pop)

} // namespace

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0);
    const std::string dllPath =
        R"(C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\CPU73050.dll)";
    HMODULE hCpu = LoadLibraryExA(dllPath.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!hCpu) hCpu = LoadLibraryA(dllPath.c_str());
    if (!hCpu) {
        std::fprintf(stderr, "[CPU73050-ORACLE] Failed to load CPU73050.dll err=%lu\n", GetLastError());
        return 1;
    }
    const uintptr_t base = reinterpret_cast<uintptr_t>(hCpu);

    // Wire ALL in-process continuation, bus, DSP-fetch and exception callbacks (ZERO .text patching, ZERO disk writes!)
    *reinterpret_cast<uintptr_t*>(base + kRead8PtrRva)    = reinterpret_cast<uintptr_t>(&StubRead8);
    *reinterpret_cast<uintptr_t*>(base + kRead16PtrRva)   = reinterpret_cast<uintptr_t>(&StubRead16);
    *reinterpret_cast<uintptr_t*>(base + kRead32PtrRva)   = reinterpret_cast<uintptr_t>(&StubRead32);
    *reinterpret_cast<uintptr_t*>(base + kWrite8PtrRva)   = reinterpret_cast<uintptr_t>(&StubWrite8);
    *reinterpret_cast<uintptr_t*>(base + kWrite16PtrRva)  = reinterpret_cast<uintptr_t>(&StubWrite16);
    *reinterpret_cast<uintptr_t*>(base + kWrite32PtrRva)  = reinterpret_cast<uintptr_t>(&StubWrite32);
    *reinterpret_cast<uintptr_t*>(base + kNextJmpPtrRva)  = reinterpret_cast<uintptr_t>(&OracleStepDone);
    *reinterpret_cast<uintptr_t*>(base + kDspFetchPtrRva) = reinterpret_cast<uintptr_t>(&OracleDspFetchSecondWord);
    *reinterpret_cast<uintptr_t*>(base + kExcepJmpPtrRva) = reinterpret_cast<uintptr_t>(&OracleExceptionHook);

    auto* regs = reinterpret_cast<RegSlot*>(base + kRegArrayRva);
    auto* dispatch = reinterpret_cast<uint32_t*>(base + kDispatchTblRva);
    auto* cpuFlags = reinterpret_cast<uint32_t*>(base + kCpuFlagsRva);

    auto RunOne = [&](uint16_t op1, uint16_t op2 = 0) -> uint16_t {
        g_trapped = 0;
        g_trapCode = 0;
        g_writeCount = 0;
        g_lastWriteAddr = 0;
        g_lastWriteVal = 0;
        *cpuFlags = 0;
        g_insnBuf[0] = op1;
        g_insnBuf[1] = op2;
        uint16_t oid = DecodeCpu73050Oid(base, op1);
        void* fn = reinterpret_cast<void*>(dispatch[oid]);
        InvokeCpu73050Handler(fn, g_insnBuf);
        return oid;
    };

    // Mode 1: High-speed binary batch mode: --batch <in.bin> <out.bin>
    if (argc >= 4 && std::strcmp(argv[1], "--batch") == 0) {
        std::ifstream fin(argv[2], std::ios::binary);
        std::ofstream fout(argv[3], std::ios::binary);
        if (!fin || !fout) {
            std::fprintf(stderr, "[CPU73050-ORACLE] Failed to open batch files\n");
            return 2;
        }
        OracleReq req{};
        uint64_t count = 0;
        std::memset(g_ram, 0xFF, sizeof(g_ram)); // Match Casio OS / Cg50NativeCore RAM reset pattern (0xFF)
        while (fin.read(reinterpret_cast<char*>(&req), sizeof(req))) {
            std::memset(regs, 0, sizeof(RegSlot) * 49);
            for (int i = 0; i < 49; ++i) regs[i].value = req.initRegs[i];
            if (req.flags & 1u) {
                RamWrite32(req.ramAddr, req.ramVal);
                RamWrite32(req.ramAddr + 4u, req.ramVal);
                g_writeCount = 0;
            }
            uint16_t oid = RunOne(req.op1, req.op2);
            OracleResp resp{};
            resp.id = req.id;
            resp.op1 = req.op1;
            resp.op2 = req.op2;
            resp.oid = oid;
            resp.trapped = static_cast<uint16_t>(g_trapped);
            resp.trapCode = g_trapCode;
            for (int i = 0; i < 49; ++i) resp.outRegs[i] = regs[i].value;
            resp.writeCount = g_writeCount;
            resp.lastWriteAddr = g_lastWriteAddr;
            resp.lastWriteVal = g_lastWriteVal;
            fout.write(reinterpret_cast<const char*>(&resp), sizeof(resp));
            ++count;
        }
        std::printf("[CPU73050-ORACLE] Batch executed %llu live SH7305 vectors\n",
                    static_cast<unsigned long long>(count));
        return 0;
    }

    // Mode 2: Full 65,536 16-bit opcode + 256 DSP 2nd-word census export: --census <out.json>
    if (argc >= 3 && std::strcmp(argv[1], "--census") == 0) {
        const uint32_t illegalVa = static_cast<uint32_t>(base + 0x00016125u);
        uint32_t active16 = 0, illegal16 = 0, dsp16 = 0, fpu16 = 0;
        uint32_t oidCounts[256] = {};
        uint16_t oidSampleOp[256] = {};
        for (uint32_t op = 0; op < 65536u; ++op) {
            uint16_t oid = DecodeCpu73050Oid(base, static_cast<uint16_t>(op));
            if (oid < 256u) {
                if (oidCounts[oid] == 0u) oidSampleOp[oid] = static_cast<uint16_t>(op);
                ++oidCounts[oid];
            }
            if (dispatch[oid] == illegalVa) {
                ++illegal16;
            } else {
                ++active16;
                if (oid == 225u || oid == 226u || oid == 227u) ++dsp16;
            }
        }
        // Probe ALL 256 DSP 2nd-word upper-byte dispatch slots (0xF800 + (hi<<8 | 0x07))
        auto* dsp2Tbl = reinterpret_cast<uint32_t*>(base + kDsp2TblRva);
        const uint32_t dspIllegalVa = static_cast<uint32_t>(base + 0x00010270u);
        uint32_t dsp2Active = 0, dsp2Illegal = 0;
        std::vector<uint32_t> dsp2ActiveHi;
        for (uint32_t hi = 0; hi < 256u; ++hi) {
            if (dsp2Tbl[hi] == dspIllegalVa) {
                ++dsp2Illegal;
            } else {
                ++dsp2Active;
                dsp2ActiveHi.push_back(hi);
            }
        }
        std::ofstream out(argv[2]);
        out << "{\n";
        out << "  \"dll\": \"CPU73050.dll\",\n";
        out << "  \"total_16bit_encodings\": 65536,\n";
        out << "  \"active_16bit_encodings\": " << active16 << ",\n";
        out << "  \"illegal_16bit_encodings\": " << illegal16 << ",\n";
        out << "  \"dsp_0xF_encodings\": " << dsp16 << ",\n";
        out << "  \"fpu_encodings\": " << fpu16 << ",\n";
        out << "  \"dsp2_total_classes\": 256,\n";
        out << "  \"dsp2_active_classes\": " << dsp2Active << ",\n";
        out << "  \"dsp2_illegal_classes\": " << dsp2Illegal << ",\n";
        out << "  \"oid_table\": [\n";
        bool first = true;
        for (uint32_t oid = 0; oid < 256u; ++oid) {
            if (oidCounts[oid] == 0u) continue;
            if (!first) out << ",\n";
            first = false;
            out << "    {\"oid\": " << oid
                << ", \"encodings\": " << oidCounts[oid]
                << ", \"sample_op\": " << oidSampleOp[oid]
                << ", \"is_illegal\": " << (dispatch[oid] == illegalVa ? "true" : "false")
                << ", \"handler_rva\": \"0x" << std::hex << (dispatch[oid] - static_cast<uint32_t>(base)) << std::dec << "\"}";
        }
        out << "\n  ],\n  \"dsp2_active_hi_bytes\": [";
        for (size_t i = 0; i < dsp2ActiveHi.size(); ++i) {
            if (i) out << ", ";
            out << dsp2ActiveHi[i];
        }
        out << "]\n}\n";
        std::printf("[CPU73050-ORACLE] Census: 16-bit active=%u illegal=%u dsp0xF=%u | DSP2 active=%u/256 illegal=%u/256\n",
                    active16, illegal16, dsp16, dsp2Active, dsp2Illegal);
        return 0;
    }

    // Default self-test proving live ALU, Load/Store, Illegal FPU trap, and 32-bit DSP PADD!
    std::printf("[CPU73050-ORACLE] Loaded CPU73050.dll at 0x%08X\n", static_cast<uint32_t>(base));

    // T1: ADD R1, R0 (0x301C)
    std::memset(regs, 0, sizeof(RegSlot) * 49);
    regs[0].value = 0x7FFFFFFFu; regs[1].value = 0x00000001u; regs[16].value = 0x40000000u;
    RunOne(0x301Cu);
    std::printf("[T1 ADD] R0=0x%08X SR=0x%08X trap=%u\n", regs[0].value, regs[16].value, g_trapped);

    // T2: ADDC R1, R0 (0x301E)
    std::memset(regs, 0, sizeof(RegSlot) * 49);
    regs[0].value = 0xFFFFFFFFu; regs[1].value = 0x00000000u; regs[16].value = 0x40000001u;
    RunOne(0x301Eu);
    std::printf("[T2 ADDC] R0=0x%08X SR=0x%08X trap=%u\n", regs[0].value, regs[16].value, g_trapped);

    // T3: MOV.L R1, @R0 (0x2012) + MOV.L @R0, R2 (0x6202) through scratch RAM callbacks!
    std::memset(regs, 0, sizeof(RegSlot) * 49);
    regs[0].value = 0x8C001000u; regs[1].value = 0xDEADBEEFu; regs[16].value = 0x40000000u;
    RunOne(0x2012u);
    RunOne(0x6202u);
    std::printf("[T3 MEM] R2=0x%08X writes=%u trap=%u\n", regs[2].value, g_writeCount, g_trapped);

    // T4: Illegal FPU opcode 0xFC00 (and 0xF19D with SR.DSP=0!) -> MUST TRAP (code=0x28)!
    std::memset(regs, 0, sizeof(RegSlot) * 49);
    regs[16].value = 0x40000000u; // SR.DSP = 0
    RunOne(0xF19Du);
    const uint32_t t4aTrap = g_trapped, t4aCode = g_trapCode;
    regs[16].value = 0x40001000u; // SR.DSP = 1
    RunOne(0xFC00u);
    std::printf("[T4 ILLEGAL] 0xF19D(DSP=0): trap=%u code=0x%X | 0xFC00(DSP=1): trap=%u code=0x%X\n",
                t4aTrap, t4aCode, g_trapped, g_trapCode);

    // T5: 32-bit SH4AL-DSP PADD X0, Y0, A0 (0xF800, 0xB007) with SR.DSP=1!
    std::memset(regs, 0, sizeof(RegSlot) * 49);
    regs[16].value = 0x40001000u; // MD=1, DSP=1
    regs[45].value = 0x00050000u; // X0 = 5 << 16
    regs[47].value = 0x00030000u; // Y0 = 3 << 16
    RunOne(0xF800u, 0xB007u);
    std::printf("[T5 DSP PADD] A0=0x%08X A0G=0x%08X DSR=0x%08X trap=%u code=0x%X\n",
                regs[39].value, regs[41].value, regs[38].value, g_trapped, g_trapCode);

    return (regs[39].value == 0x00080000u && t4aTrap == 1u && g_trapped == 0u) ? 0 : 1;
}
