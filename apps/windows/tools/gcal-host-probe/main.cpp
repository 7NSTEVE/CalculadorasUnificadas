#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

namespace {

using GcaiCallback = int(__stdcall*)(int eventCode, void* payload, void* context);
using GcaiCreateEmulator = int(__stdcall*)(
    const char* baseDirectory,
    const char* projectPath,
    GcaiCallback callback,
    void* callbackContext,
    void** emulatorHandle,
    unsigned int instanceId,
    void* reserved);
using GcaiTerminateEmulator = int(__stdcall*)(void* emulatorHandle);
using GcaiTerminateEmulatorEx = int(__stdcall*)(void* emulatorHandle, int mode);
using GcaiRequestTask = int(__stdcall*)(
    void* emulatorHandle,
    void* requestBlock,
    GcaiCallback callback,
    void* callbackContext,
    int notifyCompletion);
using GcaiCreateResidentTask = int(__stdcall*)(
    void* emulatorHandle,
    GcaiCallback callback,
    void* callbackContext);
using GcaiRequestResidentTask = int(__stdcall*)(
    void* emulatorHandle,
    void* requestBlock,
    int notifyCompletion);
using GcaiTerminateResidentTask = int(__stdcall*)(void* emulatorHandle);

// Mismo parche RAM de 7 puntos que cg50_backend.cpp PatchActivationModule()
// (EXP-014): la DLL en disco queda pristina; la activacion se neutraliza
// solo en memoria antes de que GCalAppIF consuma su estado.
bool PatchActivationModuleRam(HMODULE actMod, const char** outDetail)
{
    struct PatchEntry {
        const char* name;
        uintptr_t rva;
        const uint8_t bytes[8];
        size_t len;
    };

    static const PatchEntry kPatches[] = {
        { "AcvFx_GetInfomation2", 0x19D0, { 0x33, 0xC0, 0xC3 }, 3 },
        { "AcvFx_IsLicenseStatus", 0x2180, { 0x33, 0xC0, 0xC3 }, 3 },
        { "AcvFx_IsActivationDialog", 0x1D70, { 0x33, 0xC0, 0xC3 }, 3 },
        { "AcvFx_IsActivationDialog2", 0x1A20, { 0x33, 0xC0, 0xC3 }, 3 },
        { "AcvFx_ActivationErrorDlg", 0x21B0, { 0x33, 0xC0, 0xC2, 0x0C, 0x00 }, 5 },
        { "AcvFx_DispActivationToolMenu", 0x1F40, { 0x33, 0xC0, 0xC2, 0x04, 0x00 }, 5 },
        { "AcvFx_DispActivationToolActivation", 0x2010, { 0x33, 0xC0, 0xC2, 0x04, 0x00 }, 5 },
    };

    for (const auto& p : kPatches) {
        uint8_t* target = reinterpret_cast<uint8_t*>(actMod) + p.rva;
        DWORD oldProtect = 0;
        if (!VirtualProtect(target, p.len, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            if (outDetail) *outDetail = p.name;
            return false;
        }
        memcpy(target, p.bytes, p.len);
        VirtualProtect(target, p.len, oldProtect, &oldProtect);
        if (memcmp(target, p.bytes, p.len) != 0) {
            if (outDetail) *outDetail = p.name;
            return false;
        }
    }
    return true;
}

std::atomic<unsigned long> g_callbackCount{0};

#if defined(_M_IX86)
__declspec(naked) void __stdcall InvokeDriverSignalUpdate(
    void* /* callback */,
    void* /* signalObject */,
    unsigned int /* operation */)
{
    __asm {
        push ebx
        mov ecx, dword ptr [esp + 8]
        mov eax, dword ptr [esp + 12]
        mov ebx, dword ptr [esp + 16]
        call ecx
        pop ebx
        ret 12
    }
}
#endif

int __stdcall ProbeCallback(const int eventCode, void* const payload, void* const context)
{
    const unsigned long ordinal = ++g_callbackCount;
    std::printf(
        "CALLBACK ordinal=%lu event=%d payload=%p context=%p\n",
        ordinal,
        eventCode,
        payload,
        context);

    std::array<unsigned long, 8> words{};
    SIZE_T bytesRead = 0;
    if (payload != nullptr &&
        ReadProcessMemory(
            GetCurrentProcess(),
            payload,
            words.data(),
            sizeof(words),
            &bytesRead) &&
        bytesRead == sizeof(words)) {
        std::printf(
            "CALLBACK_DATA %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\n",
            words[0], words[1], words[2], words[3],
            words[4], words[5], words[6], words[7]);
    }
    std::fflush(stdout);
    return 0;
}

std::string ToAnsi(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(
        CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) {
        return {};
    }

    std::string result(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(
            CP_ACP,
            0,
            value.c_str(),
            -1,
            result.data(),
            required,
            nullptr,
            nullptr) == 0) {
        return {};
    }
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

std::wstring FullPath(const wchar_t* const input)
{
    DWORD required = GetFullPathNameW(input, 0, nullptr, nullptr);
    if (required == 0) {
        return {};
    }

    std::wstring result(static_cast<std::size_t>(required), L'\0');
    const DWORD written = GetFullPathNameW(input, required, result.data(), nullptr);
    if (written == 0 || written >= required) {
        return {};
    }
    result.resize(written);
    return result;
}

void PrintRelevantModules(const char* const phase)
{
    std::printf("MODULE_SNAPSHOT phase=%s\n", phase);
    const HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::printf("MODULE_ENUM_ERROR win32=%lu\n", GetLastError());
        return;
    }

    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            const std::wstring name(entry.szModule);
            if (name.find(L"GCal") != std::wstring::npos ||
                name.find(L"GCSim") != std::wstring::npos ||
                name.find(L"DLSim") != std::wstring::npos ||
                name.find(L"73050") != std::wstring::npos ||
                name.find(L"R61524") != std::wstring::npos ||
                name.find(L"1616") != std::wstring::npos ||
                name.find(L"29GL") != std::wstring::npos) {
                std::wprintf(
                    L"MODULE name=%ls base=%p path=%ls\n",
                    entry.szModule,
                    entry.modBaseAddr,
                    entry.szExePath);
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

void PumpMessagesFor(const DWORD milliseconds)
{
    const ULONGLONG end = GetTickCount64() + milliseconds;
    while (GetTickCount64() < end) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(10);
    }
}

bool DumpGcalInfoRam(const std::wstring& outputPath)
{
    constexpr std::uintptr_t infoRamPointerRva = 0x9D64;
    constexpr DWORD infoRamBytes = 0x80000;

    const HMODULE module = GetModuleHandleW(L"GCalAppIFA.dll");
    if (module == nullptr) {
        std::printf("INFO_RAM unavailable module_not_loaded\n");
        return false;
    }

    const auto pointerAddress = reinterpret_cast<const void* const*>(
        reinterpret_cast<std::uintptr_t>(module) + infoRamPointerRva);
    const void* infoRam = *pointerAddress;
    MEMORY_BASIC_INFORMATION memory{};
    if (infoRam == nullptr ||
        VirtualQuery(infoRam, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0 ||
        memory.RegionSize < infoRamBytes) {
        std::printf(
            "INFO_RAM unavailable pointer=%p region=%zu state=%lu protect=0x%lX\n",
            infoRam,
            static_cast<std::size_t>(memory.RegionSize),
            memory.State,
            memory.Protect);
        return false;
    }

    const HANDLE file = CreateFileW(
        outputPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::printf("INFO_RAM create_failed win32=%lu\n", GetLastError());
        return false;
    }

    DWORD written = 0;
    const BOOL writeResult = WriteFile(
        file, infoRam, infoRamBytes, &written, nullptr);
    CloseHandle(file);
    if (!writeResult || written != infoRamBytes) {
        std::printf(
            "INFO_RAM write_failed bytes=%lu win32=%lu\n",
            written,
            GetLastError());
        return false;
    }

    const auto* bytes = static_cast<const unsigned char*>(infoRam);
    std::uint64_t fnv = 1469598103934665603ULL;
    std::size_t nonzero = 0;
    for (DWORD index = 0; index < infoRamBytes; ++index) {
        fnv ^= bytes[index];
        fnv *= 1099511628211ULL;
        nonzero += bytes[index] != 0 ? 1U : 0U;
    }
    std::wprintf(
        L"INFO_RAM dumped path=%ls pointer=%p bytes=%lu nonzero=%zu fnv64=%016llX\n",
        outputPath.c_str(),
        infoRam,
        infoRamBytes,
        nonzero,
        static_cast<unsigned long long>(fnv));
    return true;
}

bool DumpLoadedModuleSection(
    const wchar_t* const moduleName,
    const char* const sectionName,
    const std::wstring& outputPath)
{
    const HMODULE module = GetModuleHandleW(moduleName);
    if (module == nullptr) {
        std::wprintf(L"MODULE_SECTION unavailable module=%ls\n", moduleName);
        return false;
    }

    const auto* const base = reinterpret_cast<const unsigned char*>(module);
    const auto* const dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        std::wprintf(L"MODULE_SECTION invalid_dos module=%ls\n", moduleName);
        return false;
    }
    const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(
        base + static_cast<std::size_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        std::wprintf(L"MODULE_SECTION invalid_nt module=%ls\n", moduleName);
        return false;
    }

    const IMAGE_SECTION_HEADER* selected = nullptr;
    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections;
         ++index, ++section) {
        char name[IMAGE_SIZEOF_SHORT_NAME + 1]{};
        std::memcpy(name, section->Name, IMAGE_SIZEOF_SHORT_NAME);
        if (std::strcmp(name, sectionName) == 0) {
            selected = section;
            break;
        }
    }
    if (selected == nullptr) {
        std::printf("MODULE_SECTION missing module=%ls section=%s\n",
                    moduleName,
                    sectionName);
        return false;
    }

    const auto* const data = base + selected->VirtualAddress;
    const DWORD byteCount = selected->Misc.VirtualSize;
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(data, &memory, sizeof(memory)) != sizeof(memory) ||
        memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        std::printf(
            "MODULE_SECTION unreadable module=%ls section=%s address=%p "
            "bytes=%lu state=%lu protect=0x%lX\n",
            moduleName,
            sectionName,
            data,
            byteCount,
            memory.State,
            memory.Protect);
        return false;
    }

    const HANDLE file = CreateFileW(
        outputPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::printf("MODULE_SECTION create_failed win32=%lu\n", GetLastError());
        return false;
    }

    DWORD written = 0;
    const BOOL writeResult = WriteFile(file, data, byteCount, &written, nullptr);
    CloseHandle(file);
    if (!writeResult || written != byteCount) {
        std::printf(
            "MODULE_SECTION write_failed bytes=%lu expected=%lu win32=%lu\n",
            written,
            byteCount,
            GetLastError());
        return false;
    }

    std::uint64_t fnv = 1469598103934665603ULL;
    std::size_t nonzero = 0;
    for (DWORD index = 0; index < byteCount; ++index) {
        fnv ^= data[index];
        fnv *= 1099511628211ULL;
        nonzero += data[index] != 0 ? 1U : 0U;
    }
    std::wprintf(
        L"MODULE_SECTION dumped module=%ls section=%hs path=%ls "
        L"rva=0x%08lX pointer=%p bytes=%lu nonzero=%zu fnv64=%016llX\n",
        moduleName,
        sectionName,
        outputPath.c_str(),
        selected->VirtualAddress,
        data,
        byteCount,
        nonzero,
        static_cast<unsigned long long>(fnv));
    return true;
}

void PrintLcdState()
{
    constexpr std::uintptr_t stateRva = 0xD8BD4;
    constexpr std::array<std::uintptr_t, 12> pointerRvas{
        0xD8BD8, 0xD8BDC, 0xD8BE0, 0xD8BE4,
        0x121A0, 0x121A4, 0x121D4, 0x12224,
        0x12228, 0x12244, 0x12254, 0x12264};

    const HMODULE module = GetModuleHandleW(L"LCDR61524A.dll");
    if (module == nullptr) {
        std::printf("LCD_STATE unavailable module_not_loaded\n");
        return;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    const auto flags = *reinterpret_cast<const unsigned char*>(base + stateRva);
    std::printf("LCD_STATE module=%p flags=0x%02X", module, flags);
    for (const auto rva : pointerRvas) {
        const auto value = *reinterpret_cast<const std::uint32_t*>(base + rva);
        std::printf(" rva_%05lX=%08lX",
                    static_cast<unsigned long>(rva),
                    static_cast<unsigned long>(value));
    }
    std::printf("\n");
}

bool DumpLcdImage(const std::wstring& outputPath)
{
    constexpr std::uintptr_t imageAccessorRva = 0xD8EF0;
    constexpr std::size_t pixelOffset = 0x87CC;
    constexpr DWORD surfaceWidth = 1024;
    constexpr DWORD surfaceHeight = 512;
    constexpr DWORD bytesPerPixel = 4;
    constexpr DWORD surfaceBytes =
        surfaceWidth * surfaceHeight * bytesPerPixel;
    using ImageAccessor = void*(__stdcall*)(int acquire);

    const HMODULE module = GetModuleHandleW(L"LCDR61524A.dll");
    if (module == nullptr) {
        std::printf("LCD_IMAGE unavailable module_not_loaded\n");
        return false;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    const auto accessor = *reinterpret_cast<ImageAccessor const*>(
        base + imageAccessorRva);
    if (accessor == nullptr) {
        std::printf("LCD_IMAGE unavailable accessor_null\n");
        return false;
    }

    void* const image = accessor(1);
    if (image == nullptr) {
        std::printf("LCD_IMAGE unavailable image_null accessor=%p\n",
                    reinterpret_cast<void*>(accessor));
        accessor(0);
        return false;
    }

    const auto* const imageBytes = static_cast<const unsigned char*>(image);
    const auto* const pixels = imageBytes + pixelOffset;
    MEMORY_BASIC_INFORMATION memory{};
    const bool readable =
        VirtualQuery(pixels, &memory, sizeof(memory)) == sizeof(memory) &&
        memory.State == MEM_COMMIT &&
        (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0 &&
        reinterpret_cast<std::uintptr_t>(pixels) + surfaceBytes <=
            reinterpret_cast<std::uintptr_t>(memory.BaseAddress) +
                memory.RegionSize;
    if (!readable) {
        std::printf(
            "LCD_IMAGE unreadable image=%p pixels=%p region=%p size=%zu "
            "state=%lu protect=0x%lX\n",
            image,
            pixels,
            memory.BaseAddress,
            static_cast<std::size_t>(memory.RegionSize),
            memory.State,
            memory.Protect);
        accessor(0);
        return false;
    }

    const auto contentWidth = *reinterpret_cast<const std::uint32_t*>(
        imageBytes + 0x87C4);
    const auto contentHeight = *reinterpret_cast<const std::uint32_t*>(
        imageBytes + 0x87C8);
    const HANDLE file = CreateFileW(
        outputPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::printf("LCD_IMAGE create_failed win32=%lu\n", GetLastError());
        accessor(0);
        return false;
    }

    DWORD written = 0;
    const BOOL writeResult = WriteFile(
        file, pixels, surfaceBytes, &written, nullptr);
    CloseHandle(file);

    std::uint64_t fnv = 1469598103934665603ULL;
    std::size_t nonzero = 0;
    if (writeResult && written == surfaceBytes) {
        for (DWORD index = 0; index < surfaceBytes; ++index) {
            fnv ^= pixels[index];
            fnv *= 1099511628211ULL;
            nonzero += pixels[index] != 0 ? 1U : 0U;
        }
    }
    accessor(0);

    if (!writeResult || written != surfaceBytes) {
        std::printf(
            "LCD_IMAGE write_failed bytes=%lu expected=%lu win32=%lu\n",
            written,
            surfaceBytes,
            GetLastError());
        return false;
    }
    std::wprintf(
        L"LCD_IMAGE dumped path=%ls accessor=%p image=%p pixels=%p "
        L"surface=%lux%lu content=%lux%lu bytes=%lu nonzero=%zu "
        L"fnv64=%016llX\n",
        outputPath.c_str(),
        reinterpret_cast<void*>(accessor),
        image,
        pixels,
        surfaceWidth,
        surfaceHeight,
        static_cast<unsigned long>(contentWidth),
        static_cast<unsigned long>(contentHeight),
        surfaceBytes,
        nonzero,
        static_cast<unsigned long long>(fnv));
    return true;
}

std::size_t MeasureLcdImageNonzero()
{
    constexpr std::uintptr_t imageAccessorRva = 0xD8EF0;
    constexpr std::size_t pixelOffset = 0x87CC;
    constexpr std::size_t surfaceBytes = 1024U * 512U * 4U;
    using ImageAccessor = void*(__stdcall*)(int acquire);

    const HMODULE module = GetModuleHandleW(L"LCDR61524A.dll");
    if (module == nullptr) return 0;
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    const auto accessor = *reinterpret_cast<ImageAccessor const*>(
        base + imageAccessorRva);
    if (accessor == nullptr) return 0;
    void* const image = accessor(1);
    if (image == nullptr) {
        accessor(0);
        return 0;
    }
    const auto* const pixels =
        static_cast<const unsigned char*>(image) + pixelOffset;
    std::size_t nonzero = 0;
    for (std::size_t index = 0; index < surfaceBytes; ++index) {
        nonzero += pixels[index] != 0 ? 1U : 0U;
    }
    accessor(0);
    return nonzero;
}

void PrintKeyTable()
{
    constexpr std::uintptr_t keyCountRva = 0xCD54;
    constexpr std::uintptr_t keyTableRva = 0xCD58;
    constexpr std::size_t keyEntryBytes = 0x20;
    constexpr std::uint32_t maximumReasonableKeys = 256;

    const HMODULE module = GetModuleHandleW(L"Key1616SwA.dll");
    if (module == nullptr) {
        std::printf("KEY_TABLE unavailable module_not_loaded\n");
        return;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    const auto count = *reinterpret_cast<const std::uint32_t*>(
        base + keyCountRva);
    std::printf("KEY_TABLE module=%p count=%lu\n",
                module,
                static_cast<unsigned long>(count));
    if (count > maximumReasonableKeys) {
        std::printf("KEY_TABLE rejected unreasonable_count=%lu\n",
                    static_cast<unsigned long>(count));
        return;
    }

    for (std::uint32_t index = 1; index <= count; ++index) {
        const auto* const entry = reinterpret_cast<const unsigned char*>(
            base + keyTableRva + index * keyEntryBytes);
        const auto code = *reinterpret_cast<const std::uint16_t*>(entry);
        const auto qualifier = *reinterpret_cast<const std::uint16_t*>(
            entry + 2);
        const auto mask = *reinterpret_cast<const std::uint32_t*>(entry + 4);
        const auto output = *reinterpret_cast<const std::uint32_t*>(entry + 8);
        const auto next = *reinterpret_cast<const std::uint16_t*>(entry + 0x14);
        const auto left = *reinterpret_cast<const std::int16_t*>(entry + 0x18);
        const auto top = *reinterpret_cast<const std::int16_t*>(entry + 0x1A);
        const auto right = *reinterpret_cast<const std::int16_t*>(entry + 0x1C);
        const auto bottom = *reinterpret_cast<const std::int16_t*>(entry + 0x1E);
        std::printf(
            "KEY index=%lu code=0x%04X qualifier=0x%04X mask=0x%08lX "
            "output=0x%08lX next=%u rect=%d,%d,%d,%d\n",
            static_cast<unsigned long>(index),
            code,
            qualifier,
            static_cast<unsigned long>(mask),
            static_cast<unsigned long>(output),
            next,
            left,
            top,
            right,
            bottom);
    }
}

void PrintKeyState(const char* const phase)
{
    constexpr std::uintptr_t directModeRva = 0xF1E4;
    constexpr std::uintptr_t directCodeRva = 0xCCC8;
    constexpr std::uintptr_t switchStateRva = 0xCCD4;
    constexpr std::uintptr_t columnStateRva = 0xCCAC;
    constexpr std::uintptr_t rowStateRva = 0xCCB0;

    const HMODULE module = GetModuleHandleW(L"Key1616SwA.dll");
    if (module == nullptr) {
        std::printf("KEY_STATE phase=%s unavailable module_not_loaded\n", phase);
        return;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    const auto directMode = *reinterpret_cast<const std::uintptr_t*>(
        base + directModeRva);
    std::uint32_t directModeWord = 0;
    MEMORY_BASIC_INFORMATION memory{};
    if (directMode != 0 &&
        VirtualQuery(reinterpret_cast<const void*>(directMode),
                     &memory,
                     sizeof(memory)) == sizeof(memory) &&
        memory.State == MEM_COMMIT &&
        (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0) {
        directModeWord = *reinterpret_cast<const std::uint32_t*>(directMode);
    }
    std::printf(
        "KEY_STATE phase=%s module=%p direct_mode=%p direct_word=%08lX "
        "direct_code=%08lX switches=%08lX columns=%08lX rows=%08lX\n",
        phase,
        module,
        reinterpret_cast<void*>(directMode),
        static_cast<unsigned long>(directModeWord),
        static_cast<unsigned long>(*reinterpret_cast<const std::uint32_t*>(
            base + directCodeRva)),
        static_cast<unsigned long>(*reinterpret_cast<const std::uint32_t*>(
            base + switchStateRva)),
        static_cast<unsigned long>(*reinterpret_cast<const std::uint32_t*>(
            base + columnStateRva)),
        static_cast<unsigned long>(*reinterpret_cast<const std::uint32_t*>(
            base + rowStateRva)));
}

void PrintRequestState(const char* const phase)
{
    const HMODULE dlsim = GetModuleHandleW(L"DLSim32E.dll");
    const HMODULE gcsim = GetModuleHandleW(L"CASIO GCSim.dll");
    if (dlsim == nullptr || gcsim == nullptr) {
        std::printf("REQUEST_STATE phase=%s unavailable dlsim=%p gcsim=%p\n",
                    phase,
                    dlsim,
                    gcsim);
        return;
    }
    const auto dlsimBase = reinterpret_cast<std::uintptr_t>(dlsim);
    const auto gcsimBase = reinterpret_cast<std::uintptr_t>(gcsim);
    const auto readIndex = *reinterpret_cast<const std::uint32_t*>(
        dlsimBase + 0x5EE50);
    const auto writeIndex = *reinterpret_cast<const std::uint32_t*>(
        dlsimBase + 0x5FFA0);
    const auto previousIndex = (writeIndex - 1U) & 0x3FFU;
    const auto ringCommand = *reinterpret_cast<const std::uint32_t*>(
        dlsimBase + 0x600B8 + previousIndex * sizeof(std::uint32_t));
    const auto ringPayload = *reinterpret_cast<const std::uint32_t*>(
        dlsimBase + 0x5EF80 + previousIndex * sizeof(std::uint32_t));
    const auto activeCommand = *reinterpret_cast<const std::uint32_t*>(
        gcsimBase + 0x4E74);
    const auto activePayload = *reinterpret_cast<const std::uint32_t*>(
        gcsimBase + 0x5094);
    std::printf(
        "REQUEST_STATE phase=%s read=%lu write=%lu previous=%lu "
        "ring_command=%lu ring_payload=0x%08lX active_command=%lu "
        "active_payload=0x%08lX\n",
        phase,
        static_cast<unsigned long>(readIndex),
        static_cast<unsigned long>(writeIndex),
        static_cast<unsigned long>(previousIndex),
        static_cast<unsigned long>(ringCommand),
        static_cast<unsigned long>(ringPayload),
        static_cast<unsigned long>(activeCommand),
        static_cast<unsigned long>(activePayload));
}

void PrintGcsimState(const char* const phase)
{
    const HMODULE module = GetModuleHandleW(L"CASIO GCSim.dll");
    if (module == nullptr) {
        std::printf("GCSIM_STATE phase=%s unavailable\n", phase);
        return;
    }
    constexpr std::array<std::uintptr_t, 10> stateRvas{
        0x4750, 0x4754, 0x475C, 0x4F7C, 0x4F80,
        0x4F88, 0x509C, 0x50A4, 0x50A8, 0x50B8};
    const auto base = reinterpret_cast<std::uintptr_t>(module);
    std::printf("GCSIM_STATE phase=%s module=%p", phase, module);
    for (const auto rva : stateRvas) {
        const auto value = *reinterpret_cast<const std::uint32_t*>(base + rva);
        std::printf(" rva_%04lX=%08lX",
                    static_cast<unsigned long>(rva),
                    static_cast<unsigned long>(value));
    }
    std::printf("\n");
}

void PrintAddressOwner(const char* const label, const void* const address)
{
    if (address == nullptr) {
        std::printf("ADDRESS label=%s pointer=null\n", label);
        return;
    }

    HMODULE owner = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address),
            &owner)) {
        std::printf("ADDRESS label=%s pointer=%p owner=unknown win32=%lu\n",
                    label,
                    address,
                    GetLastError());
        return;
    }

    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(owner, path, MAX_PATH);
    const auto rva = reinterpret_cast<std::uintptr_t>(address) -
                     reinterpret_cast<std::uintptr_t>(owner);
    std::wprintf(L"ADDRESS label=%hs pointer=%p owner=%ls rva=0x%08lX\n",
                 label,
                 address,
                 path,
                 static_cast<unsigned long>(rva));
}

void PrintDlsimDeviceSlots()
{
    const HMODULE dlsim = GetModuleHandleW(L"DLSim32E.dll");
    if (dlsim == nullptr) {
        std::printf("DEVICE_SLOTS unavailable\n");
        return;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(dlsim);
    for (unsigned int slot = 0; slot < 8; ++slot) {
        const auto rva = static_cast<std::uintptr_t>(0x150600 + slot * 0x10);
        const auto table = *reinterpret_cast<void***>(base + rva);
        std::printf("DEVICE_SLOT index=%u global_rva=0x%08lX table=%p\n",
                    slot,
                    static_cast<unsigned long>(rva),
                    table);
        if (table == nullptr) continue;
        for (unsigned int method = 0; method < 32; ++method) {
            char label[64]{};
            std::snprintf(label,
                          sizeof(label),
                          "slot%u_method%u",
                          slot,
                          method);
            PrintAddressOwner(label, table[method]);
        }
    }
}

std::string ReadBoundedAnsi(const std::uint32_t rawAddress)
{
    if (rawAddress == 0) return {};
    const auto* cursor = reinterpret_cast<const char*>(rawAddress);
    std::string result;
    result.reserve(80);
    for (std::size_t index = 0; index < 127; ++index) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(cursor + index, &memory, sizeof(memory)) !=
                sizeof(memory) ||
            memory.State != MEM_COMMIT ||
            (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            return {};
        }
        const unsigned char value = static_cast<unsigned char>(cursor[index]);
        if (value == 0) return result;
        if (value < 0x20 || value > 0x7E) return {};
        result.push_back(static_cast<char>(value));
    }
    return result;
}

void PrintModelDevices()
{
    using ModelGetDeviceInfo = int(__stdcall*)(void* output, unsigned int index);
    const HMODULE dlsim = GetModuleHandleW(L"DLSim32E.dll");
    const auto getDeviceInfo = dlsim == nullptr
        ? nullptr
        : reinterpret_cast<ModelGetDeviceInfo>(GetProcAddress(
              dlsim,
              "_DLSIM_ModelGetDeviceInfo@8"));
    if (getDeviceInfo == nullptr) {
        std::printf("MODEL_DEVICES unavailable\n");
        return;
    }

    for (unsigned int index = 1; index <= 2048; ++index) {
        std::array<std::uint32_t, 6> words{};
        const int result = getDeviceInfo(words.data(), index);
        const std::string deviceName = ReadBoundedAnsi(words[0]);
        const std::string terminalName = ReadBoundedAnsi(words[1]);
        std::printf(
            "MODEL_DEVICE index=%u result=%d device=%s terminal=%s "
            "words=%08lX,%08lX,%08lX,%08lX,%08lX,%08lX\n",
            index,
            result,
            deviceName.c_str(),
            terminalName.c_str(),
            static_cast<unsigned long>(words[0]),
            static_cast<unsigned long>(words[1]),
            static_cast<unsigned long>(words[2]),
            static_cast<unsigned long>(words[3]),
            static_cast<unsigned long>(words[4]),
            static_cast<unsigned long>(words[5]));
        if (result == 0) break;
    }
}

void PrintInternalModelRecord(const unsigned int index)
{
    const HMODULE dlsim = GetModuleHandleW(L"DLSim32E.dll");
    if (dlsim == nullptr) return;
    const auto base = reinterpret_cast<std::uintptr_t>(dlsim);
    const auto record = *reinterpret_cast<const std::uint32_t*>(
        base + 0x7B1B8 + index * sizeof(std::uint32_t));
    std::printf("MODEL_RECORD index=%u address=%08lX\n",
                index,
                static_cast<unsigned long>(record));
    if (record == 0) return;
    for (unsigned int offset = 0; offset < 0x80; offset += 4) {
        const auto value = *reinterpret_cast<const std::uint32_t*>(record + offset);
        const std::string text = ReadBoundedAnsi(value);
        std::printf("MODEL_RECORD index=%u offset=%02X value=%08lX text=%s\n",
                    index,
                    offset,
                    static_cast<unsigned long>(value),
                    text.c_str());
    }

    const auto driverIndex = *reinterpret_cast<const std::uint32_t*>(
        record + 0x44);
    const auto driverRecord = *reinterpret_cast<const std::uint32_t*>(
        base + 0xCB9C8 + driverIndex * sizeof(std::uint32_t));
    std::printf(
        "MODEL_DRIVER index=%u driver_index=%lu driver_record=%08lX\n",
        index,
        static_cast<unsigned long>(driverIndex),
        static_cast<unsigned long>(driverRecord));
    if (driverRecord == 0) return;
    for (unsigned int offset = 0; offset < 0x80; offset += 4) {
        const auto value = *reinterpret_cast<const std::uint32_t*>(
            driverRecord + offset);
        const std::string text = ReadBoundedAnsi(value);
        std::printf("MODEL_DRIVER index=%u offset=%02X value=%08lX text=%s\n",
                    index,
                    offset,
                    static_cast<unsigned long>(value),
                    text.c_str());
    }
}

void PrintPowerContext(const char* const phase)
{
    const HMODULE dlsim = GetModuleHandleW(L"DLSim32E.dll");
    if (dlsim == nullptr) return;
    const auto base = reinterpret_cast<std::uintptr_t>(dlsim);
    constexpr unsigned int powerModelIndex = 48;
    const auto record = *reinterpret_cast<const std::uint32_t*>(
        base + 0x7B1B8 + powerModelIndex * sizeof(std::uint32_t));
    if (record == 0) return;
    const auto driverIndex = *reinterpret_cast<const std::uint32_t*>(
        record + 0x44);
    const auto driverRecord = *reinterpret_cast<const std::uint32_t*>(
        base + 0xCB9C8 + driverIndex * sizeof(std::uint32_t));
    if (driverRecord == 0) return;
    const auto context = *reinterpret_cast<const std::uint32_t*>(
        driverRecord + 0x1C);
    std::printf("POWER_CONTEXT phase=%s context=%08lX",
                phase,
                static_cast<unsigned long>(context));
    if (context == 0) {
        std::printf("\n");
        return;
    }
    for (unsigned int offset = 0x20; offset <= 0x60; offset += 4) {
        const auto value = *reinterpret_cast<const std::uint32_t*>(
            context + offset);
        std::printf(" off_%02X=%08lX",
                    offset,
                    static_cast<unsigned long>(value));
    }
    std::printf("\n");
}

void PrintCpuState(const char* const phase)
{
    const HMODULE cpu = GetModuleHandleW(L"CPU73050.dll");
    if (cpu == nullptr) return;
    const auto base = reinterpret_cast<std::uintptr_t>(cpu);
    constexpr std::array<std::uintptr_t, 10> stateRvas{
        0x675430, 0x675434, 0x67543C, 0x6754C4, 0x6754D0,
        0x675A18, 0x675A1C, 0x675AB8, 0x675AC0, 0x368BC0};
    std::printf("CPU_STATE phase=%s module=%p", phase, cpu);
    for (const auto rva : stateRvas) {
        const auto value = *reinterpret_cast<const std::uint32_t*>(base + rva);
        std::printf(" rva_%06lX=%08lX",
                    static_cast<unsigned long>(rva),
                    static_cast<unsigned long>(value));
    }
    std::printf("\n");
}

void PrintDlsimStateAndError(const char* const phase)
{
    const HMODULE dlsim = GetModuleHandleW(L"DLSim32E.dll");
    if (dlsim == nullptr) return;
    const auto base = reinterpret_cast<std::uintptr_t>(dlsim);
    using DlsimErrorText = const char*(__stdcall*)();
    const auto errorText = reinterpret_cast<DlsimErrorText>(GetProcAddress(
        dlsim, "_DLSIM_ErrorText@0"));
    const char* textValue = nullptr;
    if (errorText != nullptr) {
        __try {
            textValue = errorText();
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            textValue = nullptr;
        }
    }
    std::printf(
        "DLSIM_STATE phase=%s model_state=%08lX error_code=%08lX "
        "error_flags=%08lX error_text=%s\n",
        phase,
        static_cast<unsigned long>(
            *reinterpret_cast<const std::uint32_t*>(base + 0x1D3A88)),
        static_cast<unsigned long>(
            *reinterpret_cast<const std::uint32_t*>(base + 0x6E66C)),
        static_cast<unsigned long>(
            *reinterpret_cast<const std::uint32_t*>(base + 0x6E668)),
        textValue == nullptr ? "<null>" : textValue);
}

void PrintCpuMemoryAtResetVector(const char* const phase)
{
#if defined(_M_IX86)
    const HMODULE dlsim = GetModuleHandleW(L"DLSim32E.dll");
    if (dlsim == nullptr) {
        std::printf("CPU_MEMORY phase=%s unavailable reason=no-dlsim\n", phase);
        return;
    }

    const auto dlsimBase = reinterpret_cast<std::uintptr_t>(dlsim);
    void** const cpuMethods = *reinterpret_cast<void***>(dlsimBase + 0x150600);
    if (cpuMethods == nullptr || cpuMethods[3] == nullptr ||
        cpuMethods[5] == nullptr) {
        std::printf(
            "CPU_MEMORY phase=%s unavailable table=%p get_state=%p read=%p\n",
            phase,
            cpuMethods,
            cpuMethods == nullptr ? nullptr : cpuMethods[3],
            cpuMethods == nullptr ? nullptr : cpuMethods[5]);
        return;
    }

    using CpuGetAddressState = unsigned int(__stdcall*)(std::uint32_t* state);
    using CpuReadMemory = unsigned int(__stdcall*)(
        const std::uint32_t* state,
        unsigned int accessFlags,
        std::uint32_t* value);
    const auto getAddressState =
        reinterpret_cast<CpuGetAddressState>(cpuMethods[3]);
    const auto readMemory = reinterpret_cast<CpuReadMemory>(cpuMethods[5]);

    // CPU73050 method 3 returns the current PC plus its address-space flags.
    // Method 5 accepts that same two-dword address structure.  0x0A00 asks
    // for a 16-bit instruction-side read, matching the SH-4A instruction
    // fetch call sites inside CPU73050.dll.
    std::uint32_t currentAddress[2]{};
    unsigned int stateFlags = 0;
    __try {
        stateFlags = getAddressState(currentAddress);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::printf(
            "CPU_MEMORY phase=%s exception=get-state code=0x%08lX\n",
            phase,
            static_cast<unsigned long>(GetExceptionCode()));
        return;
    }

    std::printf(
        "CPU_MEMORY phase=%s pc=%08lX address_flags=%08lX state_flags=%08lX",
        phase,
        static_cast<unsigned long>(currentAddress[0]),
        static_cast<unsigned long>(currentAddress[1]),
        static_cast<unsigned long>(stateFlags));

    for (unsigned int offset = 0; offset < 32; offset += 2) {
        std::uint32_t address[2]{
            currentAddress[0] + offset,
            currentAddress[1]};
        std::uint32_t value = 0xDEADBEEF;
        unsigned int readFlags = 0;
        bool readCompleted = false;
        __try {
            readFlags = readMemory(address, 0x0A00, &value);
            readCompleted = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            std::printf(
                " off_%02X=EXC:%08lX",
                offset,
                static_cast<unsigned long>(GetExceptionCode()));
        }
        if (readCompleted) {
            std::printf(" off_%02X=%04lX/%04lX",
                        offset,
                        static_cast<unsigned long>(value & 0xFFFFU),
                        static_cast<unsigned long>(readFlags & 0xFFFFU));
        }
    }
    std::printf("\n");
#else
    std::printf("CPU_MEMORY phase=%s unavailable reason=non-x86\n", phase);
#endif
}

void PrintCpuMemoryMapProbes(const char* const phase)
{
#if defined(_M_IX86)
    const HMODULE dlsim = GetModuleHandleW(L"DLSim32E.dll");
    if (dlsim == nullptr) return;
    const auto dlsimBase = reinterpret_cast<std::uintptr_t>(dlsim);
    void** const cpuMethods = *reinterpret_cast<void***>(dlsimBase + 0x150600);
    if (cpuMethods == nullptr || cpuMethods[5] == nullptr) return;
    using CpuReadMemory = unsigned int(__stdcall*)(
        const std::uint32_t* state,
        unsigned int accessFlags,
        std::uint32_t* value);
    const auto readMemory = reinterpret_cast<CpuReadMemory>(cpuMethods[5]);
    struct ProbeAddress {
        std::uint32_t address;
        std::uint32_t addressFlags;
    };
    constexpr std::array<ProbeAddress, 10> probes{{
        {0xA0000000, 0x60000000},
        {0xA0000000, 0x00000000},
        {0x00000000, 0x60000000},
        {0x00000000, 0x00000000},
        {0x80000000, 0x60000000},
        {0x80000000, 0x00000000},
        {0xA1000000, 0x00000000},
        {0x0C000000, 0x00000000},
        {0x14000000, 0x00000000},
        {0xFF000000, 0x00000000},
    }};
    std::printf("CPU_MAP_PROBE phase=%s", phase);
    for (const auto& probe : probes) {
        const std::uint32_t addressState[2]{probe.address, probe.addressFlags};
        std::uint32_t value = 0xDEADBEEF;
        unsigned int result = 0;
        bool completed = false;
        __try {
            result = readMemory(addressState, 0x0A00, &value);
            completed = true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            std::printf(" %08lX:%08lX=EXC:%08lX",
                        static_cast<unsigned long>(probe.address),
                        static_cast<unsigned long>(probe.addressFlags),
                        static_cast<unsigned long>(GetExceptionCode()));
        }
        if (completed) {
            std::printf(" %08lX:%08lX=%04lX/%04lX",
                        static_cast<unsigned long>(probe.address),
                        static_cast<unsigned long>(probe.addressFlags),
                        static_cast<unsigned long>(value & 0xFFFFU),
                        static_cast<unsigned long>(result & 0xFFFFU));
        }
    }
    std::printf("\n");
#else
    (void)phase;
#endif
}

void PrintKeySignalState()
{
    const HMODULE key = GetModuleHandleW(L"Key1616SwA.dll");
    if (key == nullptr) return;
    const auto base = reinterpret_cast<std::uintptr_t>(key);
    constexpr std::array<std::uintptr_t, 8> signalPointerRvas{
        0xBB14, 0xBB38, 0xBB5C, 0xBB80,
        0xBBA4, 0xBBC8, 0xBBEC, 0xBC10};
    std::printf("KEY_SIGNAL callbacks update=%p flush=%p\n",
                *reinterpret_cast<void**>(base + 0xEFF8),
                *reinterpret_cast<void**>(base + 0xEFF0));
    PrintAddressOwner("key_signal_update",
                      *reinterpret_cast<void**>(base + 0xEFF8));
    PrintAddressOwner("key_signal_flush",
                      *reinterpret_cast<void**>(base + 0xEFF0));
    for (unsigned int index = 0; index < signalPointerRvas.size(); ++index) {
        const auto object = *reinterpret_cast<unsigned char**>(
            base + signalPointerRvas[index]);
        if (object == nullptr) {
            std::printf("KEY_SIGNAL index=%u object=null\n", index);
            continue;
        }
        std::printf(
            "KEY_SIGNAL index=%u object=%p flags=%02X value=%08lX aux=%08lX\n",
            index,
            object,
            object[6],
            static_cast<unsigned long>(
                *reinterpret_cast<const std::uint32_t*>(object + 8)),
            static_cast<unsigned long>(
                *reinterpret_cast<const std::uint32_t*>(object + 12)));
    }
}

bool SetKeyboardResetSignal(const bool asserted)
{
#if defined(_M_IX86)
    const HMODULE key = GetModuleHandleW(L"Key1616SwA.dll");
    if (key == nullptr) return false;
    const auto base = reinterpret_cast<std::uintptr_t>(key);
    auto* const signalObject = *reinterpret_cast<unsigned char**>(base + 0xBB14);
    void* const updateCallback = *reinterpret_cast<void**>(base + 0xEFF8);
    const auto flushCallback = *reinterpret_cast<void(__cdecl**)()>(base + 0xEFF0);
    if (signalObject == nullptr || updateCallback == nullptr ||
        flushCallback == nullptr) {
        std::printf(
            "RESET_SIGNAL unavailable asserted=%d object=%p update=%p flush=%p\n",
            asserted ? 1 : 0,
            signalObject,
            updateCallback,
            reinterpret_cast<void*>(flushCallback));
        return false;
    }

    // This reproduces Key1616SwA's own SW0A branch at RVA 0x248B:
    // value 0 asserts the active-low reset line, value 1 releases it.
    signalObject[6] = asserted ? 0x01 : 0x09;
    *reinterpret_cast<std::uint32_t*>(signalObject + 8) = asserted ? 0U : 1U;
    InvokeDriverSignalUpdate(updateCallback, signalObject, 2);
    flushCallback();
    std::printf(
        "RESET_SIGNAL asserted=%d object=%p flags=%02X value=%08lX\n",
        asserted ? 1 : 0,
        signalObject,
        signalObject[6],
        static_cast<unsigned long>(
            *reinterpret_cast<const std::uint32_t*>(signalObject + 8)));
    return true;
#else
    (void)asserted;
    std::printf("RESET_SIGNAL unavailable non_x86_build\n");
    return false;
#endif
}

// F1.3 (2026-10-16): vuelca una ventana de memoria GUEST del CPU73050 en
// ejecución mediante method3 (GetAddressState) + method5 (lectura 16-bit,
// accessFlags 0x0A00), usando los address-flags vivos del PC actual. Sirve
// para desensamblar el bucle de sondeo del OS en 0x8013832A-36.
// Los bloques SEH viven en helpers sin objetos C++ (C2712).
unsigned int GuestGetAddressStateSeh(void* const getAddressStateFn,
                                     std::uint32_t* const currentAddress)
{
    using CpuGetAddressState = unsigned int(__stdcall*)(std::uint32_t*);
    const auto getAddressState =
        reinterpret_cast<CpuGetAddressState>(getAddressStateFn);
    __try {
        return getAddressState(currentAddress);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        currentAddress[0] = 0;
        currentAddress[1] = 0;
        return 0xFFFFFFFFU;
    }
}

// Real caller cut (CPU73050.dll RVA 0x354B): method5(state, accessFlags,
// &outValue) -> status, with state = { dword address; dword flags; } and
// the state buffer treated as at least 16 dwords by the generic path.
// accessFlags: 0x900 = byte, 0xA00 = word; status bit 0x1000 = rejected.
// The {address, flags} pair from method3 uses flags 0x60000000 that the
// fast path rejects, so combos are swept for the valid one.
unsigned long GuestReadWordSeh(void* const readMemoryFn,
                               const std::uint32_t pairFlags,
                               const std::uint32_t accessFlags,
                               const std::uint32_t address,
                               std::uint32_t* const valueOut,
                               std::uint32_t* const statusOut)
{
    using CpuReadMemory = unsigned int(__stdcall*)(
        const std::uint32_t* state,
        unsigned int accessFlags,
        std::uint32_t* value);
    const auto readMemory = reinterpret_cast<CpuReadMemory>(readMemoryFn);
    std::uint32_t state[16]{};
    state[0] = address;
    state[1] = pairFlags;
    std::uint32_t value = 0xDEADBEEF;
    std::uint32_t status = 0xEEEEEEEEU;
    __try {
        status = readMemory(state, accessFlags, &value);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *statusOut = 0xFFFFFFFFU;
        return 1;
    }
    *valueOut = value;
    *statusOut = status;
    return 0;
}

unsigned long GuestReadLoopSeh(void* const readMemoryFn,
                               const std::uint32_t pairFlags,
                               const std::uint32_t accessFlags,
                               const std::uint32_t start,
                               unsigned char* const buffer,
                               const std::size_t size,
                               unsigned long* const errorCountOut,
                               std::uint32_t* const firstStatusOut)
{
    using CpuReadMemory = unsigned int(__stdcall*)(
        const std::uint32_t* state,
        unsigned int accessFlags,
        std::uint32_t* value);
    const auto readMemory = reinterpret_cast<CpuReadMemory>(readMemoryFn);
    unsigned long exceptions = 0;
    unsigned long errors = 0;
    std::uint32_t firstStatus = 0xEEEEEEEEU;
    bool haveStatus = false;
    for (std::size_t off = 0; off + 1 < size; off += 2) {
        std::uint32_t state[16]{};
        state[0] = start + static_cast<std::uint32_t>(off);
        state[1] = pairFlags;
        std::uint32_t value = 0xDEADBEEF;
        std::uint32_t status = 0xEEEEEEEEU;
        __try {
            status = readMemory(state, accessFlags, &value);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            ++exceptions;
            continue;
        }
        if (!haveStatus) {
            firstStatus = status;
            haveStatus = true;
        }
        if ((status & 0x1000U) != 0 || status == 0xFFFFFFFFU) {
            ++errors;
        }
        buffer[off] = static_cast<unsigned char>(value & 0xFFU);
        buffer[off + 1] = static_cast<unsigned char>((value >> 8) & 0xFFU);
    }
    *errorCountOut = errors;
    *firstStatusOut = firstStatus;
    return exceptions;
}

bool DumpGuestMemoryWindow(const std::uint32_t start,
                           const std::size_t size,
                           const std::wstring& outputPath)
{
#if defined(_M_IX86)
    const HMODULE dlsim = GetModuleHandleW(L"DLSim32E.dll");
    if (dlsim == nullptr) {
        std::printf("GUEST_DUMP unavailable reason=no-dlsim\n");
        return false;
    }
    const auto dlsimBase = reinterpret_cast<std::uintptr_t>(dlsim);
    void** const cpuMethods = *reinterpret_cast<void***>(dlsimBase + 0x150600);
    if (cpuMethods == nullptr || cpuMethods[3] == nullptr ||
        cpuMethods[5] == nullptr) {
        std::printf("GUEST_DUMP unavailable table=%p get=%p read=%p\n",
                    static_cast<void*>(cpuMethods),
                    cpuMethods == nullptr ? nullptr : cpuMethods[3],
                    cpuMethods == nullptr ? nullptr : cpuMethods[5]);
        return false;
    }

    const HMODULE cpuModule = GetModuleHandleW(L"CPU73050.dll");
    if (cpuModule != nullptr) {
        const auto cpuBase = reinterpret_cast<std::uintptr_t>(cpuModule);
        const std::uint32_t modeGlobal =
            *reinterpret_cast<const std::uint32_t*>(cpuBase + 0x677B78);
        const std::uint32_t pcGlobal =
            *reinterpret_cast<const std::uint32_t*>(cpuBase + 0x675A18);
        std::printf(
            "GUEST_DUMP cpu_globals mode_10677B78=%08lX pc_10675A18=%08lX\n",
            static_cast<unsigned long>(modeGlobal),
            static_cast<unsigned long>(pcGlobal));
    }

    std::uint32_t currentAddress[2]{};
    const unsigned int stateFlags =
        GuestGetAddressStateSeh(cpuMethods[3], currentAddress);
    if (stateFlags == 0xFFFFFFFFU) {
        std::printf("GUEST_DUMP exception=get-state\n");
        return false;
    }
    std::printf(
        "GUEST_DUMP pc=%08lX address_flags=%08lX state_flags=%08lX "
        "window=%08lX+%zu\n",
        static_cast<unsigned long>(currentAddress[0]),
        static_cast<unsigned long>(currentAddress[1]),
        static_cast<unsigned long>(stateFlags),
        static_cast<unsigned long>(start),
        size);

    struct SweepCombo {
        std::uint32_t pairFlags;
        std::uint32_t accessFlags;
    };
    const SweepCombo combos[] = {
        {0x60000000U, 0x00000A00U},
        {0x60000000U, 0x10000A00U},
        {0xE0000000U, 0x00000A00U},
        {0xE0000000U, 0x10000A00U},
        {0x00000000U, 0x10000A00U},
        {0x00000000U, 0x00000A00U},
        {0x80000000U, 0x10000A00U},
        {0x60000000U, 0x00000900U},
    };
    std::uint32_t bestPairFlags = 0x60000000U;
    std::uint32_t bestAccessFlags = 0x00000A00U;
    bool bestFound = false;
    long bestScore = -1;
    for (std::size_t comboIndex = 0;
         comboIndex < sizeof(combos) / sizeof(combos[0]);
         ++comboIndex) {
        const std::uint32_t probeAddress =
            currentAddress[0] != 0 ? currentAddress[0] : start;
        std::uint32_t words[8]{};
        std::uint32_t firstStatus = 0xEEEEEEEEU;
        unsigned long errors = 0;
        unsigned long exceptions = 0;
        bool allZero = true;
        for (unsigned int index = 0; index < 8; ++index) {
            std::uint32_t value = 0xDEADBEEF;
            std::uint32_t status = 0xEEEEEEEEU;
            exceptions += GuestReadWordSeh(cpuMethods[5],
                                           combos[comboIndex].pairFlags,
                                           combos[comboIndex].accessFlags,
                                           probeAddress + index * 2,
                                           &value,
                                           &status);
            if (index == 0) {
                firstStatus = status;
            }
            if ((status & 0x1000U) != 0 || status == 0xFFFFFFFFU) {
                ++errors;
            }
            words[index] = value;
            if (value != 0 && value != 0xDEADBEEF) {
                allZero = false;
            }
        }
        std::printf(
            "GUEST_DUMP sweep[%zu] flags=%08lX acc=%08lX status0=%08lX "
            "errors=%lu exc=%lu w=%04lX %04lX %04lX %04lX %04lX %04lX %04lX %04lX\n",
            comboIndex,
            static_cast<unsigned long>(combos[comboIndex].pairFlags),
            static_cast<unsigned long>(combos[comboIndex].accessFlags),
            static_cast<unsigned long>(firstStatus),
            errors,
            exceptions,
            static_cast<unsigned long>(words[0]),
            static_cast<unsigned long>(words[1]),
            static_cast<unsigned long>(words[2]),
            static_cast<unsigned long>(words[3]),
            static_cast<unsigned long>(words[4]),
            static_cast<unsigned long>(words[5]),
            static_cast<unsigned long>(words[6]),
            static_cast<unsigned long>(words[7]));
        long score = allZero ? 0 : 100;
        score -= static_cast<long>(errors);
        score -= static_cast<long>(exceptions) * 2;
        if (score > bestScore) {
            bestScore = score;
            bestPairFlags = combos[comboIndex].pairFlags;
            bestAccessFlags = combos[comboIndex].accessFlags;
            bestFound = !allZero && errors == 0;
        }
    }
    std::printf(
        "GUEST_DUMP using flags=%08lX acc=%08lX real=%d\n",
        static_cast<unsigned long>(bestPairFlags),
        static_cast<unsigned long>(bestAccessFlags),
        bestFound ? 1 : 0);

    std::vector<unsigned char> buffer(size, 0);
    unsigned long errors = 0;
    std::uint32_t firstStatus = 0xEEEEEEEEU;
    const unsigned long exceptions =
        GuestReadLoopSeh(cpuMethods[5],
                         bestPairFlags,
                         bestAccessFlags,
                         start,
                         buffer.data(),
                         size,
                         &errors,
                         &firstStatus);

    HANDLE file = CreateFileW(outputPath.c_str(),
                              GENERIC_WRITE,
                              0,
                              nullptr,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::printf("GUEST_DUMP create_failed win32=%lu\n",
                    static_cast<unsigned long>(GetLastError()));
        return false;
    }
    DWORD written = 0;
    const BOOL writeOk =
        WriteFile(file,
                  buffer.data(),
                  static_cast<DWORD>(size),
                  &written,
                  nullptr);
    CloseHandle(file);
    std::printf(
        "GUEST_DUMP path=%ls written=%lu ok=%d exceptions=%lu "
        "errors=%lu status0=%08lX\n",
        outputPath.c_str(),
        static_cast<unsigned long>(written),
        writeOk ? 1 : 0,
        exceptions,
        errors,
        static_cast<unsigned long>(firstStatus));
    for (std::size_t off = 0; off < size && off < 0x180; off += 16) {
        std::printf("GUEST_DUMP %08lX ",
                    static_cast<unsigned long>(start + off));
        for (std::size_t i = 0; i < 16 && off + i < size; ++i) {
            std::printf("%02X", buffer[off + i]);
        }
        std::printf("\n");
    }
    return writeOk && written == size;
#else
    (void)start;
    (void)size;
    (void)outputPath;
    std::printf("GUEST_DUMP unavailable non_x86_build\n");
    return false;
#endif
}


} // namespace

static LONG CALLBACK ProbeVehHandler(EXCEPTION_POINTERS* info)
{
    const auto* rec = info->ExceptionRecord;
    const DWORD code = rec->ExceptionCode;
    if (code == 0x80000001 /*GUARD_PAGE*/ || code == 0xC0000005 /*ACCESS_VIOLATION*/ ||
        code == 0xC000001D /*ILLEGAL_INSTRUCTION*/ || code == 0xC0000096 /*PRIV_INSTR*/ ||
        code == 0xC0000006 /*IN_PAGE_ERROR*/) {
        const ULONG_PTR opKind = rec->NumberParameters > 0 ? rec->ExceptionInformation[0] : 0;
        const ULONG_PTR faultAddr = rec->NumberParameters > 1 ? rec->ExceptionInformation[1] : 0;
        std::fprintf(stderr, "VEH code=0x%08lX addr=%p op=%lu mem=%p thread=%lu\n",
                     static_cast<unsigned long>(code),
                     rec->ExceptionAddress,
                     static_cast<unsigned long>(opKind),
                     reinterpret_cast<void*>(faultAddr),
                     GetCurrentThreadId());
        {
            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(faultAddr), &mbi, sizeof(mbi)) != 0) {
                std::fprintf(stderr,
                             "VEH mem_region base=%p size=%zx state=%lx protect=%lx alloc_base=%p\n",
                             mbi.BaseAddress,
                             static_cast<std::size_t>(mbi.RegionSize),
                             static_cast<unsigned long>(mbi.State),
                             static_cast<unsigned long>(mbi.Protect),
                             mbi.AllocationBase);
            }
            if (info->ContextRecord != nullptr &&
                VirtualQuery(info->ContextRecord, &mbi, sizeof(mbi)) != 0) {
                std::fprintf(stderr, "VEH ctx_region base=%p size=%zx state=%lx protect=%lx\n",
                             mbi.BaseAddress,
                             static_cast<std::size_t>(mbi.RegionSize),
                             static_cast<unsigned long>(mbi.State),
                             static_cast<unsigned long>(mbi.Protect));
            }
        }
        if (info->ContextRecord != nullptr) {
            const auto ctx = info->ContextRecord;
            std::fprintf(stderr,
                         "VEH regs eip=%08lX esp=%08lX ebp=%08lX eax=%08lX ebx=%08lX ecx=%08lX edx=%08lX esi=%08lX edi=%08lX\n",
                         static_cast<unsigned long>(ctx->Eip),
                         static_cast<unsigned long>(ctx->Esp),
                         static_cast<unsigned long>(ctx->Ebp),
                         static_cast<unsigned long>(ctx->Eax),
                         static_cast<unsigned long>(ctx->Ebx),
                         static_cast<unsigned long>(ctx->Ecx),
                         static_cast<unsigned long>(ctx->Edx),
                         static_cast<unsigned long>(ctx->Esi),
                         static_cast<unsigned long>(ctx->Edi));
            unsigned char code16[16]{};
            SIZE_T read = 0;
            if (ReadProcessMemory(GetCurrentProcess(),
                                  reinterpret_cast<LPCVOID>(ctx->Eip), code16,
                                  sizeof(code16), &read) != 0 && read > 0) {
                std::fprintf(stderr, "VEH code_at_eip=");
                for (SIZE_T i = 0; i < read; ++i) {
                    std::fprintf(stderr, "%02X", code16[i]);
                }
                std::fprintf(stderr, "\n");
            }
        }
        std::fflush(stderr);
        HMODULE mod = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(rec->ExceptionAddress), &mod)) {
            wchar_t path[MAX_PATH]{};
            GetModuleFileNameW(mod, path, MAX_PATH);
            const wchar_t* name = wcsrchr(path, L'\\');
            name = name == nullptr ? path : name + 1;
            std::fprintf(stderr, "VEH module=%ls rva=0x%08lX\n", name,
                         static_cast<unsigned long>(
                             reinterpret_cast<std::uintptr_t>(rec->ExceptionAddress) -
                             reinterpret_cast<std::uintptr_t>(mod)));
        }
        void* frames[64]{};
        const USHORT count = RtlCaptureStackBackTrace(0, 64, frames, nullptr);
        for (USHORT i = 0; i < count; ++i) {
            HMODULE fm = nullptr;
            wchar_t name[64] = L"?";
            unsigned long rva = 0;
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   reinterpret_cast<LPCWSTR>(frames[i]), &fm)) {
                wchar_t path[MAX_PATH]{};
                GetModuleFileNameW(fm, path, MAX_PATH);
                const wchar_t* leaf = wcsrchr(path, L'\\');
                leaf = leaf == nullptr ? path : leaf + 1;
                _snwprintf_s(name, 64, _TRUNCATE, L"%ls", leaf);
                rva = static_cast<unsigned long>(
                    reinterpret_cast<std::uintptr_t>(frames[i]) -
                    reinterpret_cast<std::uintptr_t>(fm));
            }
            std::fprintf(stderr, "VEH frame[%02u] %p %ls+0x%lX\n",
                         static_cast<unsigned>(i), frames[i], name, rva);
        }
        std::fflush(stderr);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static bool TryCopyPageFromTable(uint8_t* dest, uintptr_t entryAddr)
{
    __try {
        const uintptr_t hostRaw = *reinterpret_cast<const uintptr_t*>(entryAddr + 4);
        const uint8_t* const hostPtr = reinterpret_cast<const uint8_t*>(hostRaw & ~3U);
        if (hostPtr != nullptr) {
            std::memcpy(dest, hostPtr, 0x1000U);
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return false;
}

static void TryInspectDescriptors(uintptr_t tablePtrAddr)
{
    __try {
        uintptr_t* const table = *reinterpret_cast<uintptr_t**>(tablePtrAddr);
        std::printf("CPU_ROM desc_table=%p\n", reinterpret_cast<void*>(table));
        if (table != nullptr) {
            for (unsigned int i = 0; i < 32; ++i) {
                const uintptr_t desc = table[i];
                if (desc == 0) continue;
                const uint32_t flags = *reinterpret_cast<const uint32_t*>(desc + 0x08);
                const uint32_t guestAddr = *reinterpret_cast<const uint32_t*>(desc + 0x54);
                const uint32_t guestSize = *reinterpret_cast<const uint32_t*>(desc + 0x5C);
                const uint32_t seed = *reinterpret_cast<const uint32_t*>(desc + 0x6C);
                std::printf("CPU_ROM_DESC index=%u desc=%p guest_addr=0x%08X guest_size=0x%08X seed=0x%08X flags=0x%08X\n",
                            i, reinterpret_cast<void*>(desc), guestAddr, guestSize, seed, flags);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::printf("CPU_ROM table_exc=0x%08lX\n",
                    static_cast<unsigned long>(GetExceptionCode()));
    }
}

bool DumpRawRomRegion(
    const char* label,
    const std::wstring& outputPath,
    uintptr_t pageTableBase,
    uint32_t startAddr,
    uint32_t totalBytes)
{
    const uint32_t pageCount = totalBytes / 0x1000U;
    std::vector<uint8_t> buffer(totalBytes, 0);
    uint32_t validPages = 0;
    uint32_t missingPages = 0;

    for (uint32_t p = 0; p < pageCount; ++p) {
        const uint32_t addr = startAddr + p * 0x1000U;
        const uint32_t pageIdx = (addr >> 12) & 0x1FFFFU;
        const uintptr_t entryAddr = pageTableBase + pageIdx * 8U;
        uint8_t* dest = buffer.data() + p * 0x1000U;

        if (TryCopyPageFromTable(dest, entryAddr)) {
            ++validPages;
        } else {
            ++missingPages;
        }
    }

    std::printf(
        "ROM_DUMP %s pages_total=%u pages_valid=%u pages_missing=%u\n",
        label, pageCount, validPages, missingPages);

    if (validPages == 0) {
        std::printf("ROM_DUMP %s failed: no valid pages found\n", label);
        return false;
    }

    const HANDLE file = CreateFileW(
        outputPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        std::printf("ROM_DUMP %s create_failed win32=%lu\n", label, GetLastError());
        return false;
    }

    DWORD written = 0;
    const BOOL writeResult = WriteFile(
        file, buffer.data(), static_cast<DWORD>(buffer.size()), &written, nullptr);
    CloseHandle(file);
    if (!writeResult || written != buffer.size()) {
        std::printf(
            "ROM_DUMP %s write_failed written=%lu total=%zu win32=%lu\n",
            label, written, buffer.size(), GetLastError());
        return false;
    }

    std::uint64_t fnv = 1469598103934665603ULL;
    std::size_t nonzero = 0;
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        fnv ^= buffer[i];
        fnv *= 1099511628211ULL;
        if (buffer[i] != 0) ++nonzero;
    }

    char hexSample[48] = {0};
    std::snprintf(hexSample, sizeof(hexSample),
                  "%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                  buffer[0], buffer[1], buffer[2], buffer[3],
                  buffer[4], buffer[5], buffer[6], buffer[7],
                  buffer[8], buffer[9], buffer[10], buffer[11],
                  buffer[12], buffer[13], buffer[14], buffer[15]);

    std::wprintf(
        L"ROM_DUMP %hs dumped path=%ls bytes=%zu nonzero=%zu fnv64=%016llX sample=[%hs]\n",
        label, outputPath.c_str(), buffer.size(), nonzero,
        static_cast<unsigned long long>(fnv), hexSample);
    return true;
}

void InspectAndDumpCpuRom(const char* const phase, const std::wstring& runtime)
{
    const HMODULE cpuMod = GetModuleHandleW(L"CPU73050.dll");
    if (cpuMod == nullptr) {
        std::printf("CPU_ROM phase=%s module_not_found\n", phase);
        return;
    }
    const uintptr_t cpuBase = reinterpret_cast<uintptr_t>(cpuMod);
    const uintptr_t tablePtrAddr = cpuBase + 0x68638CU;
    std::printf("CPU_ROM phase=%s cpu_base=%p table_ptr_addr=%p\n",
                phase, reinterpret_cast<void*>(cpuBase), reinterpret_cast<void*>(tablePtrAddr));

    TryInspectDescriptors(tablePtrAddr);

    const uintptr_t pageTableBase = cpuBase + 0xCEFE0U;
    std::printf("CPU_ROM phase=%s page_table_base=%p\n", phase, reinterpret_cast<void*>(pageTableBase));

    // Dump SystemROM: [0xA0000000, 0xA0B60000), 0x00B60000 bytes
    DumpRawRomRegion("SystemROM", runtime + L"\\SystemROM.raw", pageTableBase, 0xA0000000U, 0x00B60000U);

    // Dump UserROM: [0xA1000000, 0xA24A0000), 0x014A0000 bytes
    DumpRawRomRegion("UserROM", runtime + L"\\UserROM.raw", pageTableBase, 0xA1000000U, 0x014A0000U);
}

void ScanForRomPattern(const char* const phase)
{
    const unsigned char pat[6] = {0xdf, 0x23, 0x7f, 0xfc, 0xd2, 0x23};
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    unsigned char* p = nullptr;
    MEMORY_BASIC_INFORMATION mbi{};
    std::size_t hits = 0;
    while (p < si.lpMaximumApplicationAddress) {
        if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) break;
        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) == 0 &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READ |
                            PAGE_EXECUTE_READWRITE)) != 0 &&
            mbi.RegionSize >= 0x1000) {
            const unsigned char* base =
                static_cast<const unsigned char*>(mbi.BaseAddress);
            const std::size_t lim =
                mbi.RegionSize > 0x8000000 ? 0x8000000 : mbi.RegionSize;
            __try {
                for (std::size_t i = 0; i + 8 <= lim; i += 1) {
                    if (base[i] == pat[0] && base[i+1] == pat[1] &&
                        base[i+2] == pat[2] && base[i+3] == pat[3] &&
                        base[i+4] == pat[4] && base[i+5] == pat[5]) {
                        std::printf("ROM_PATTERN phase=%s region=%p size=%zx offset=%zx protect=%lx\n",
                                    phase, mbi.BaseAddress,
                                    static_cast<std::size_t>(mbi.RegionSize), i,
                                    static_cast<unsigned long>(mbi.Protect));
                        if (++hits > 6) break;
                    }
                }
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                std::printf("ROM_PATTERN phase=%s region=%p scan_exception=0x%08lX\n",
                            phase, mbi.BaseAddress,
                            static_cast<unsigned long>(GetExceptionCode()));
            }
        }
        p = static_cast<unsigned char*>(mbi.BaseAddress) + mbi.RegionSize;
    }
    std::printf("ROM_PATTERN phase=%s hits=%zu\n", phase, hits);
}

int wmain(const int argc, wchar_t** const argv)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    AddVectoredExceptionHandler(1, ProbeVehHandler);
    std::printf("GCAL_HOST_PROBE pointer_bits=%zu pid=%lu\n",
                sizeof(void*) * 8,
                GetCurrentProcessId());

    if constexpr (sizeof(void*) != 4) {
        std::printf("RESULT unsupported_architecture expected=32 actual=%zu\n",
                    sizeof(void*) * 8);
        return 20;
    }
    if (argc < 2) {
        std::printf(
            "USAGE GCalHostProbe.exe <runtime-directory> [hold-ms] "
            "[precreate-ms] [key-code]\n");
        return 21;
    }

    std::wstring runtime = FullPath(argv[1]);
    if (runtime.empty()) {
        std::printf("RESULT invalid_runtime_path win32=%lu\n", GetLastError());
        return 22;
    }
    while (!runtime.empty() &&
           (runtime.back() == L'\\' || runtime.back() == L'/')) {
        runtime.pop_back();
    }

    DWORD holdMs = 3000;
    if (argc >= 3) {
        const unsigned long parsed = std::wcstoul(argv[2], nullptr, 10);
        if (parsed > 0 && parsed <= 60000) {
            holdMs = parsed;
        }
    }
    DWORD precreateMs = 0;
    if (argc >= 4) {
        const unsigned long parsed = std::wcstoul(argv[3], nullptr, 10);
        if (parsed <= 60000) {
            precreateMs = parsed;
        }
    }
    unsigned long diagnosticKeyCode = 0;
    bool diagnosticKeySpecified = false;
    bool diagnosticKeyScan = false;
    bool diagnosticResetSignal = false;
    bool diagnosticPowerCommand = false;
    bool diagnosticPowerFirst = false;
    bool diagnosticPowerLegacy = false;
    bool diagnosticDumpGuest = false;
    bool diagnosticResident0e = false;
    // F1-D (Servulus, 2026-10-21): res0ekey = flujo res0e completo + secuencia
    // de teclas GCAI tras el MAIN MENU, con dumps por tecla y al final (oraculo
    // oficial del bug aritmetico F1-A). Codigos = matriz (input<<8)|output:
    // '1'=0x0703 '+'=0x0403 '2'=0x0603 EXE=0x0302 (decimales abajo).
    bool diagnosticResident0eKeys = false;
    std::vector<unsigned long> res0eKeyList = {1795, 1027, 1539, 770};
    std::uint32_t guestDumpStart = 0x80138000;
    std::size_t guestDumpSize = 0x1000;
    if (argc >= 5) {
        diagnosticKeyScan = _wcsicmp(argv[4], L"scan") == 0;
        diagnosticResetSignal = _wcsicmp(argv[4], L"sw0a") == 0;
        diagnosticPowerCommand = _wcsicmp(argv[4], L"power") == 0;
        diagnosticPowerFirst = _wcsicmp(argv[4], L"powerfirst") == 0;
        diagnosticPowerLegacy = _wcsicmp(argv[4], L"power1") == 0;
        diagnosticDumpGuest = _wcsicmp(argv[4], L"dumpguest") == 0;
        diagnosticResident0e = _wcsicmp(argv[4], L"res0e") == 0;
        diagnosticResident0eKeys = _wcsicmp(argv[4], L"res0ekey") == 0;
        if (diagnosticResident0eKeys) diagnosticResident0e = true;
        if (diagnosticResident0eKeys && argc >= 6) {
            res0eKeyList.clear();
            const wchar_t* p = argv[5];
            while (*p != L'\0') {
                while (*p == L',') ++p;
                if (*p == L'\0') break;
                wchar_t* end = nullptr;
                const unsigned long v = std::wcstoul(p, &end, 0);
                if (end == p) break;
                res0eKeyList.push_back(v);
                p = end;
            }
        }
        if ((diagnosticDumpGuest || diagnosticResident0e) &&
            !diagnosticResident0eKeys && argc >= 6) {
            guestDumpStart =
                static_cast<std::uint32_t>(std::wcstoul(argv[5], nullptr, 16));
        }
        if ((diagnosticDumpGuest || diagnosticResident0e) &&
            !diagnosticResident0eKeys && argc >= 7) {
            const unsigned long parsedSize =
                std::wcstoul(argv[6], nullptr, 16);
            if (parsedSize > 0 && parsedSize <= 0x40000) {
                guestDumpSize = parsedSize;
            }
        }
        if (diagnosticResident0e && argc < 6) {
            // Estado observado tras completar la tarea 0xE: el OS queda en
            // 0xA00206FC. Volcar esa ventana P2 por defecto.
            guestDumpStart = 0xA0020000;
        }
        if (!diagnosticKeyScan && !diagnosticResetSignal &&
            !diagnosticPowerCommand && !diagnosticPowerFirst &&
            !diagnosticPowerLegacy && !diagnosticDumpGuest &&
            !diagnosticResident0e && !diagnosticResident0eKeys) {
            diagnosticKeyCode = std::wcstoul(argv[4], nullptr, 0);
            diagnosticKeySpecified = true;
        }
        // dumpguest/res0e necesitan el OS corriendo: reutilizan el flujo
        // del power oficial (0x10 + AC/ON).
        if (diagnosticDumpGuest || diagnosticResident0e) {
            diagnosticPowerCommand = true;
        }
    }

    const std::wstring dllPath = runtime + L"\\GCalAppIF.dll";
    const std::wstring projectPath =
        runtime +
        L"\\fx-CG50 Manager PLUS Subscription\\EmulatorData\\CGM.dlp";
    std::wstring basePath = runtime + L"\\";

    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesW(projectPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wprintf(L"RESULT missing_input dll=%ls project=%ls\n",
                     dllPath.c_str(),
                     projectPath.c_str());
        return 23;
    }

    std::wprintf(L"INPUT runtime=%ls\n", runtime.c_str());
    std::wprintf(L"INPUT project=%ls\n", projectPath.c_str());

    if (!SetCurrentDirectoryW(runtime.c_str())) {
        std::printf("RESULT set_current_directory_failed win32=%lu\n", GetLastError());
        return 24;
    }
    if (!SetDllDirectoryW(runtime.c_str())) {
        std::printf("RESULT set_dll_directory_failed win32=%lu\n", GetLastError());
        return 25;
    }

    // EXP-014: precargar y parchear ActivationFxMb.dll ANTES de GCalAppIF,
    // como hace el backend (cg50_backend.cpp). El probe dependia antes del
    // parche en disco; ahora valida el diseño documentado: pristino + RAM.
    const std::wstring actPath = runtime + L"\\ActivationFxMb.dll";
    const HMODULE actModule = LoadLibraryW(actPath.c_str());
    if (actModule == nullptr) {
        std::printf("RESULT load_activation_failed win32=%lu\n", GetLastError());
        return 28;
    }
    const char* patchFail = nullptr;
    if (!PatchActivationModuleRam(actModule, &patchFail)) {
        std::printf("RESULT activation_patch_failed at=%s win32=%lu\n",
                    patchFail ? patchFail : "?", GetLastError());
        FreeLibrary(actModule);
        return 29;
    }
    std::printf("ACTIVATION patched_ram=7/7 module=%ls\n", actPath.c_str());

    const HMODULE gcaiModule = LoadLibraryW(dllPath.c_str());
    if (gcaiModule == nullptr) {
        std::printf("RESULT load_gcai_failed win32=%lu\n", GetLastError());
        return 26;
    }

    const auto createEmulator = reinterpret_cast<GcaiCreateEmulator>(
        GetProcAddress(gcaiModule, "_GCAI_CreateEmulator@28"));
    const auto terminateEmulator = reinterpret_cast<GcaiTerminateEmulator>(
        GetProcAddress(gcaiModule, "_GCAI_TerminateEmulator@4"));
    const auto terminateEmulatorEx = reinterpret_cast<GcaiTerminateEmulatorEx>(
        GetProcAddress(gcaiModule, "_GCAI_TerminateEmulatorEx@8"));
    const auto requestTask = reinterpret_cast<GcaiRequestTask>(
        GetProcAddress(gcaiModule, "_GCAI_RequestTask@20"));
    const auto createResidentTask = reinterpret_cast<GcaiCreateResidentTask>(
        GetProcAddress(gcaiModule, "_GCAI_CreateResidentTask@12"));
    const auto requestResidentTask = reinterpret_cast<GcaiRequestResidentTask>(
        GetProcAddress(gcaiModule, "_GCAI_RequestResidentTask@12"));
    const auto terminateResidentTask = reinterpret_cast<GcaiTerminateResidentTask>(
        GetProcAddress(gcaiModule, "_GCAI_TerminateResidentTask@4"));
    if (createEmulator == nullptr || terminateEmulator == nullptr ||
        requestTask == nullptr || createResidentTask == nullptr ||
        requestResidentTask == nullptr || terminateResidentTask == nullptr) {
        std::printf(
            "RESULT missing_exports create=%p terminate=%p request=%p "
            "create_resident=%p request_resident=%p terminate_resident=%p win32=%lu\n",
                    reinterpret_cast<void*>(createEmulator),
                    reinterpret_cast<void*>(terminateEmulator),
                    reinterpret_cast<void*>(requestTask),
                    reinterpret_cast<void*>(createResidentTask),
                    reinterpret_cast<void*>(requestResidentTask),
                    reinterpret_cast<void*>(terminateResidentTask),
                    GetLastError());
        FreeLibrary(gcaiModule);
        return 27;
    }

    const std::string baseAnsi = ToAnsi(basePath);
    const std::string projectAnsi = ToAnsi(projectPath);
    if (baseAnsi.empty() || projectAnsi.empty()) {
        std::printf("RESULT ansi_conversion_failed win32=%lu\n", GetLastError());
        FreeLibrary(gcaiModule);
        return 28;
    }

    void* emulatorHandle = nullptr;
    if (precreateMs != 0) {
        std::printf("CREATE delay_ms=%lu\n", precreateMs);
        std::fflush(stdout);
        Sleep(precreateMs);
    }
    const ULONGLONG createStarted = GetTickCount64();
    std::printf("CREATE begin instance=0x51A1\n");
    std::fflush(stdout);
    const int createResult = createEmulator(
        baseAnsi.c_str(),
        projectAnsi.c_str(),
        ProbeCallback,
        reinterpret_cast<void*>(0x43475052), // 'CGPR' diagnostic marker
        &emulatorHandle,
        0x51A1,
        nullptr);
    const ULONGLONG createElapsed = GetTickCount64() - createStarted;

    std::printf(
        "CREATE result=%d handle=%p elapsed_ms=%llu callbacks=%lu\n",
        createResult,
        emulatorHandle,
        static_cast<unsigned long long>(createElapsed),
        g_callbackCount.load());
    PrintRelevantModules("after-create");

    int exitCode = 0;
    if (createResult == 0 && emulatorHandle != nullptr) {
        // IDs 1 and 2 are handled internally by GCalAppIF.  DLSim's request
        // pump recognizes resident task ID 3 as the command channel used by
        // the Manager; other IDs are callback channels and do not enter the
        // GCSim dispatcher.
        constexpr std::uintptr_t residentTaskId = 3;
        const int residentCreateResult = createResidentTask(
            reinterpret_cast<void*>(residentTaskId),
            ProbeCallback,
            reinterpret_cast<void*>(0x52455349)); // 'RESI'
        std::printf("RESIDENT_CREATE id=%zu result=%d\n",
                    static_cast<std::size_t>(residentTaskId),
                    residentCreateResult);

        // Slots del host oficial para power-on (0x10) y reset (0x12).
        // Verificado en los call sites del exe original (RVA 0xD365/0xD3E5):
        // sin CreateResidentTask previo, el despachador 0x34A0 de GCalAppIF
        // no halla el slot y devuelve 2 sin enviar nada (hallazgo 2026-10-16).
        constexpr std::uintptr_t powerTaskId = 0x10;
        constexpr std::uintptr_t resetTaskId = 0x12;
        const int powerCreateResult = createResidentTask(
            reinterpret_cast<void*>(powerTaskId),
            ProbeCallback,
            reinterpret_cast<void*>(0x504F5731)); // 'POW1'
        std::printf("RESIDENT_CREATE id=0x%X result=%d\n",
                    static_cast<unsigned>(powerTaskId),
                    powerCreateResult);
        const int resetCreateResult = createResidentTask(
            reinterpret_cast<void*>(resetTaskId),
            ProbeCallback,
            reinterpret_cast<void*>(0x52535431)); // 'RST1'
        std::printf("RESIDENT_CREATE id=0x%X result=%d\n",
                    static_cast<unsigned>(resetTaskId),
                    resetCreateResult);

        // F1.5 (2026-10-16): el OS del CG50 queda en el bucle
        // 0x80138328-36 reintentando un servicio del host (jsr al puntero
        // de la tabla RAM 0x8041AA9C con r4=0xE, r5=0xA) hasta que el host
        // complete la tarea residente 0xE. El exe oficial la crea al
        // arrancar con CreateResidentTask(0xe, despachador, objeto). Sin
        // ese slot el servicio nunca termina y el OS reintenta para
        // siempre (causa del bucle observado 2026-10-16).
        constexpr std::uintptr_t checkTaskId = 0xE;
        if (diagnosticResident0e) {
            const int checkCreateResult = createResidentTask(
                reinterpret_cast<void*>(checkTaskId),
                ProbeCallback,
                reinterpret_cast<void*>(0x54534B30)); // 'TSK0'
            std::printf("RESIDENT_CREATE id=0x%X result=%d\n",
                        static_cast<unsigned>(checkTaskId),
                        checkCreateResult);
        }

        // GCSim starts paused. Its request dispatcher uses command 5 to clear
        // the pause flags and enter the DLSIM_CPUExecute loop.
        std::array<unsigned long, 64> startRequest{};
        startRequest[0] = 5;
        PumpMessagesFor(750);
        const ULONGLONG requestStarted = GetTickCount64();
        int requestResult = 5;
        unsigned long requestAttempts = 0;
        for (; residentCreateResult == 0 &&
               requestAttempts < 20 && requestResult != 0;
             ++requestAttempts) {
            requestResult = requestResidentTask(
                reinterpret_cast<void*>(residentTaskId),
                startRequest.data(),
                1);
            if (requestResult != 0) {
                PumpMessagesFor(100);
            }
        }
        std::printf(
            "REQUEST command=5 result=%d attempts=%lu elapsed_ms=%llu callbacks=%lu\n",
            requestResult,
            requestAttempts,
            static_cast<unsigned long long>(GetTickCount64() - requestStarted),
            g_callbackCount.load());
        PumpMessagesFor(750);
        if (diagnosticResident0e) {
            // 2026-10-16: ráfaga de muestreo de PC cada 20 ms para mapear la
            // ruta que ejecuta el OS al salir del bucle 0x80138328-36 (justo
            // después de que la tarea 0xE responde 0 al servicio). Imprime
            // solo los cambios de PC: las regiones visitadas identifican el
            // siguiente estado del arranque (idle 0xA00206xx, menú, etc.).
            const HMODULE cpuBurst = GetModuleHandleW(L"CPU73050.dll");
            const auto pcBurst = cpuBurst == nullptr
                ? nullptr
                : reinterpret_cast<const volatile std::uint32_t*>(
                      reinterpret_cast<std::uintptr_t>(cpuBurst) + 0x675A18);
            unsigned long lastPc = 0;
            std::printf("PC_TRACE begin\n");
            std::fflush(stdout);
            for (int tick = 0; tick < 160; ++tick) {
                PumpMessagesFor(20);
                const unsigned long pc = pcBurst == nullptr ? 0 : *pcBurst;
                if (pc != lastPc) {
                    std::printf("PC_TRACE tick=%d t_ms=%d pc=%08lX\n",
                                tick, (tick + 1) * 20, pc);
                    lastPc = pc;
                }
            }
            std::printf("PC_TRACE end lcd_nonzero=%zu callbacks=%lu\n",
                        MeasureLcdImageNonzero(),
                        g_callbackCount.load());
            std::fflush(stdout);
        }
        if (diagnosticPowerFirst) {
            std::printf("POWERFIRST begin\n");
            std::fflush(stdout);
            std::array<unsigned long, 64> pfReq{};
            pfReq[0] = 1;
            pfReq[1] = 1;
            pfReq[2] = 1;
            const int pf = requestResidentTask(
                reinterpret_cast<void*>(residentTaskId), pfReq.data(), 3);
            std::printf("POWERFIRST result=%d\n", pf);
            std::fflush(stdout);
            for (int i = 0; i < 10; ++i) {
                PumpMessagesFor(1000);
                std::printf("POWERFIRST t=%ds lcd_nonzero=%zu\n",
                            i + 1, MeasureLcdImageNonzero());
                std::fflush(stdout);
            }
            PrintCpuMemoryAtResetVector("powerfirst-10s");
            PrintCpuState("powerfirst-10s");
            std::printf("POWERFIRST end\n");
            std::fflush(stdout);
            return 0;
        }
        if (diagnosticPowerCommand || diagnosticPowerLegacy) {
            // POWER-on del host oficial: RequestResidentTask(slot 0x10, {0}, 0)
            // (exe original RVA 0xD365). El modo legacy "power1" reproduce el
            // intento antiguo {1,1,1} al slot 3 (comando 1) para el experimento
            // A/B del crash 0x80000001 (hallazgo 2026-10-16).
            std::printf(diagnosticPowerLegacy
                            ? "POWER legacy {1,1,1} -> slot 3 sending\n"
                            : "POWER command 0x10 {0} -> slot 0x10 sending\n");
            std::array<unsigned long, 64> powerReq{};
            int pr = 0;
            if (diagnosticPowerLegacy) {
                powerReq[0] = 1;
                powerReq[1] = 1;
                powerReq[2] = 1;
                pr = requestResidentTask(
                    reinterpret_cast<void*>(residentTaskId),
                    powerReq.data(),
                    3);
            } else {
                pr = requestResidentTask(
                    reinterpret_cast<void*>(powerTaskId),
                    powerReq.data(),
                    0);
            }
            std::printf("POWER command result=%d\n", pr);
            PumpMessagesFor(3000);
            ScanForRomPattern("power-0x10-a");
            std::size_t nzPow = MeasureLcdImageNonzero();
            std::printf("POWER lcd_nonzero_after_cmd=%zu\n", nzPow);
            const HMODULE dlsimModuleP = GetModuleHandleW(L"DLSim32E.dll");
            const auto keyPressP = dlsimModuleP == nullptr
                ? nullptr
                : reinterpret_cast<void(__stdcall*)(int, int)>(GetProcAddress(
                      dlsimModuleP, "_DLSIM_GCAIKeyPress@8"));
            if (keyPressP != nullptr) {
                std::printf("POWER pulsando AC/ON 0x0101\n");
                keyPressP(1, 0x0101);
                PumpMessagesFor(1500);
                keyPressP(2, 0);
                PumpMessagesFor(1000);
                nzPow = MeasureLcdImageNonzero();
                std::printf("POWER lcd_nonzero_after_ac=%zu\n", nzPow);
            }
        }
        PrintKeyTable();
        PrintKeyState("before-inject");
        PrintKeySignalState();
        PrintDlsimDeviceSlots();
        PrintModelDevices();
        PrintInternalModelRecord(1);
        PrintInternalModelRecord(40);
        PrintInternalModelRecord(41);
        PrintInternalModelRecord(47);
        PrintInternalModelRecord(48);
        PrintPowerContext("before-inject");
        PrintDlsimStateAndError("before-inject");
        PrintCpuState("before-inject");
        PrintCpuMemoryAtResetVector("before-inject");
        PrintCpuMemoryMapProbes("before-inject");
        PumpMessagesFor(100);
        PrintCpuState("before-inject-plus-100ms");
        PrintRequestState("before-inject");
        PrintGcsimState("before-inject");
        if (diagnosticKeySpecified || diagnosticKeyScan || diagnosticResetSignal ||
            diagnosticPowerCommand) {
            using DlsimGcaiKeyPress = void(__stdcall*)(int mode, int keyCode);
            const HMODULE dlsimModule = GetModuleHandleW(L"DLSim32E.dll");
            const auto keyPress = dlsimModule == nullptr
                ? nullptr
                : reinterpret_cast<DlsimGcaiKeyPress>(GetProcAddress(
                      dlsimModule,
                      "_DLSIM_GCAIKeyPress@8"));
            if (diagnosticPowerCommand) {
                std::printf("POWER command 0x10 {0} -> slot 0x10 sending (repeticion)\n");
                std::array<unsigned long, 64> powerReq{};
                const int pr = requestResidentTask(
                    reinterpret_cast<void*>(powerTaskId),
                    powerReq.data(),
                    0);
                std::printf("POWER command result=%d\n", pr);
                PumpMessagesFor(3000);
                std::fflush(stdout);
                // Escanear la memoria del proceso buscando el patron del firmware
                // real (primeros bytes del SystemROM del fx-CG50).
                ScanForRomPattern("power-0x10-b");
                PrintPowerContext("after-power-0x10");
                PrintCpuState("after-power-0x10");
                PrintCpuMemoryAtResetVector("after-power-0x10");
                std::size_t nz = MeasureLcdImageNonzero();
                std::printf("POWER lcd_nonzero=%zu\n", nz);
                if (keyPress != nullptr) {
                    std::printf("POWER pulsando AC/ON 0x0101\n");
                    keyPress(1, 0x0101);
                    PumpMessagesFor(1200);
                    keyPress(2, 0);
                    PumpMessagesFor(800);
                    PrintPowerContext("after-ac");
                    PrintCpuState("after-ac");
                    nz = MeasureLcdImageNonzero();
                    std::printf("POWER after-ac lcd_nonzero=%zu\n", nz);
                }
            } else if (diagnosticResetSignal) {
                std::printf("RESET_SIGNAL direct_down\n");
                SetKeyboardResetSignal(true);
                PumpMessagesFor(350);
                PrintKeySignalState();
                PrintPowerContext("reset-down-held");
                PrintCpuState("reset-down-held");
                PrintCpuMemoryAtResetVector("reset-down-held");
                SetKeyboardResetSignal(false);
                std::printf("RESET_SIGNAL direct_up\n");
                PumpMessagesFor(1000);
                PrintPowerContext("reset-released");
                PrintCpuState("reset-released");
            } else if (keyPress == nullptr) {
                std::printf("KEY_INJECT unavailable code=0x%04lX\n",
                            diagnosticKeyCode);
            } else if (diagnosticKeyScan) {
                constexpr std::array<unsigned int, 11> outputLines{
                    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12};
                std::size_t nonzero = MeasureLcdImageNonzero();
                std::printf("KEY_SCAN begin initial_nonzero=%zu\n", nonzero);
                bool displayActivated = nonzero != 0;
                unsigned int detectedCode = 0;
                for (unsigned int inputLine = 1;
                     inputLine <= 7 && !displayActivated;
                     ++inputLine) {
                    for (const unsigned int outputLine : outputLines) {
                        const unsigned int code =
                            (inputLine << 8U) | outputLine;
                        keyPress(1, static_cast<int>(code));
                        PumpMessagesFor(140);
                        keyPress(2, 0);
                        PumpMessagesFor(100);
                        nonzero = MeasureLcdImageNonzero();
                        std::printf(
                            "KEY_SCAN code=0x%04X input=%u output=%u "
                            "nonzero=%zu\n",
                            code,
                            inputLine,
                            outputLine,
                            nonzero);
                        if (nonzero != 0) {
                            displayActivated = true;
                            detectedCode = code;
                            break;
                        }
                    }
                }
                std::printf(
                    "KEY_SCAN result=%s detected_code=0x%04X nonzero=%zu\n",
                    displayActivated ? "display-active" : "no-display",
                    detectedCode,
                    nonzero);
            } else {
                std::printf("KEY_INJECT direct_down code=0x%04lX\n",
                            diagnosticKeyCode);
                keyPress(1, static_cast<int>(diagnosticKeyCode));
            }
            if (!diagnosticKeyScan && !diagnosticResetSignal) {
                // AC/ON is sampled by the power/low-power circuitry and a short
                // desktop click is not a reliable diagnostic.  Keep an explicit
                // probe key asserted long enough to exercise that path.
                PumpMessagesFor(2000);
                PrintKeyState("key-down-held");
                PrintRequestState("key-down-held");
                if (keyPress != nullptr) {
                    keyPress(2, 0);
                    std::printf("KEY_INJECT direct_up code=0x%04lX\n",
                                diagnosticKeyCode);
                }
                PumpMessagesFor(160);
            }
            PrintKeyState("after-inject");
            PrintRequestState("after-inject");
        }
        PumpMessagesFor(holdMs);
        if (diagnosticResident0e) {
            for (int sample = 0; sample < 8; ++sample) {
                PumpMessagesFor(500);
                PrintCpuState("res0e-sample");
                const std::size_t nzSample = MeasureLcdImageNonzero();
                std::printf("RES0E sample=%d lcd_nonzero=%zu callbacks=%lu\n",
                            sample,
                            nzSample,
                            g_callbackCount.load());
                std::fflush(stdout);
            }
            // F1.5 etapa 2 (2026-10-16): replicar la secuencia del host
            // oficial posterior al arranque (traza dedup t=7.19-7.88):
            //  - crear tareas 6,8,9,0xC,0xD,0xF,0x11,0x13,0x14,0x15 y slot2
            //    (0x10/0x12 ya existen) con sus requests de arranque,
            //  - requests del slot 2 (tarea UI, cmds 3 con dimensiones),
            //  - SetSDCardPath + IsSDCardPathSettingDone.
            constexpr std::uintptr_t stage2Ids[] = {
                0x6, 0x8, 0x9, 0xC, 0xD, 0xF, 0x11, 0x13, 0x14, 0x15};
            constexpr std::uintptr_t stage2RequestIds[] = {
                0x6, 0x8, 0x9, 0xC, 0xD, 0xF, 0x15};
            for (const std::uintptr_t id : stage2Ids) {
                const int batchResult = createResidentTask(
                    reinterpret_cast<void*>(id),
                    ProbeCallback,
                    reinterpret_cast<void*>(0x54534B40 + id));
                std::printf("RES0E create id=0x%X result=%d\n",
                            static_cast<unsigned>(id), batchResult);
                for (const std::uintptr_t requestId : stage2RequestIds) {
                    if (requestId != id) {
                        continue;
                    }
                    std::array<unsigned long, 64> bootReq{};
                    const int requestResult = requestResidentTask(
                        reinterpret_cast<void*>(id), bootReq.data(), 0);
                    std::printf("RES0E request id=0x%X result=%d\n",
                                static_cast<unsigned>(id), requestResult);
                }
            }
            const int slot2Create = createResidentTask(
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(2)),
                ProbeCallback,
                reinterpret_cast<void*>(0x55495431)); // 'UIT1'
            std::printf("RES0E create id=0x2 result=%d\n", slot2Create);

            // F1.5 etapa 3 (2026-10-16): el protocolo del slot 2 fue
            // decodificado en GCalAppIF.dll (RVA 0x20E0 -> 0x18F0):
            //  - cmd3 copia 8 dwords del payload al estado UI en 0x10006834
            //    y exige que +0xc (ancho), +0x10 (alto) y +0x18 (=payload[6],
            //    un HWND real; se le hace GetDC) sean distintos de cero; si
            //    alguno es cero devuelve 6.
            //  - cmd2 pasa payload[7] como HDC opcional a 0x18F0.
            //  - cmd0/1 solo alternan flags del estado.
            // Los payloads siguientes copian los del manager oficial
            // (secuencia-boot-oficial.txt lineas 236/237/240/242/243/244).
            const wchar_t* const probeClass = L"GCalProbeUIClass";
            WNDCLASSW windowClass{};
            windowClass.lpfnWndProc = DefWindowProcW;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.lpszClassName = probeClass;
            RegisterClassW(&windowClass);
            const HWND probeHwnd = CreateWindowExW(
                0, probeClass, L"GCalProbeUI", WS_OVERLAPPED,
                0, 0, 396, 224, nullptr, nullptr,
                GetModuleHandleW(nullptr), nullptr);
            const HDC probeDc =
                probeHwnd != nullptr ? GetDC(probeHwnd) : nullptr;
            const unsigned long hwndBits =
                static_cast<unsigned long>(
                    reinterpret_cast<std::uintptr_t>(probeHwnd));
            const unsigned long hdcBits =
                static_cast<unsigned long>(
                    reinterpret_cast<std::uintptr_t>(probeDc));
            std::printf("RES0E stage3 hwnd=%p hdc=%p\n",
                        probeHwnd, probeDc);
            std::fflush(stdout);

            const auto sendSlot2 =
                [&](const char* const label,
                    std::array<unsigned long, 64> payload) {
                    const int slotResult = requestResidentTask(
                        reinterpret_cast<void*>(
                            static_cast<std::uintptr_t>(2)),
                        payload.data(),
                        0);
                    std::printf(
                        "RES0E slot2 %s cmd=%lu w=%lu h=%lu d6=%08lX "
                        "result=%d\n",
                        label,
                        payload[0],
                        payload[3],
                        payload[4],
                        payload[6],
                        slotResult);
                    std::fflush(stdout);
                    PumpMessagesFor(250);
                };
            {
                std::array<unsigned long, 64> req{};
                req[0] = 3;
                req[3] = 0x80;
                req[4] = 0x40;
                req[6] = hwndBits;
                sendSlot2("A-cmd3-128x64", req);
            }
            {
                std::array<unsigned long, 64> req{};
                sendSlot2("B-cmd0", req);
            }

            using GcaiSetSDCardPath = int(__stdcall*)(const char*);
            using GcaiIsSDCardPathSettingDone = int(__stdcall*)(void*);
            const auto setSdPath = reinterpret_cast<GcaiSetSDCardPath>(
                GetProcAddress(gcaiModule, "_GCAI_SetSDCardPath@4"));
            const auto isSdDone =
                reinterpret_cast<GcaiIsSDCardPathSettingDone>(
                    GetProcAddress(gcaiModule,
                                   "_GCAI_IsSDCardPathSettingDone@4"));
            if (setSdPath != nullptr) {
                const std::string sdPath =
                    ToAnsi(runtime) +
                    "\\fx-CG50 Manager PLUS Subscription\\EmulatorData\\SDCard";
                const int setResult = setSdPath(sdPath.c_str());
                std::printf("RES0E SetSDCardPath(%s) result=%d\n",
                            sdPath.c_str(), setResult);
                if (isSdDone != nullptr) {
                    const int doneResult = isSdDone(nullptr);
                    std::printf("RES0E IsSDCardPathSettingDone result=%d\n",
                                doneResult);
                }
            } else {
                std::printf("RES0E SetSDCardPath export missing\n");
            }
            {
                std::array<unsigned long, 64> req{};
                req[0] = 3;
                req[3] = 0x18C;
                req[4] = 0xE0;
                req[5] = 0xFFFFFFFE;
                req[6] = hwndBits;
                sendSlot2("C-cmd3-396x224", req);
            }
            {
                std::array<unsigned long, 64> req{};
                req[0] = 1;
                sendSlot2("D-cmd1", req);
            }
            {
                std::array<unsigned long, 64> req{};
                req[0] = 2;
                req[3] = 0x18C;
                req[4] = 0xE0;
                req[5] = 0x451000;
                req[6] = hwndBits;
                req[7] = hdcBits;
                sendSlot2("E-cmd2", req);
            }
            {
                std::array<unsigned long, 64> req{};
                sendSlot2("F-cmd0", req);
            }
            {
                // El despachador real es GCalAppIF.dll (GCalAppIFA.dll es
                // el DLDriver y su .data 0x9D64 guarda el puntero InfoRam).
                const HMODULE gcaiDiag =
                    GetModuleHandleW(L"GCalAppIF.dll");
                if (gcaiDiag != nullptr) {
                    const auto gcaiBase =
                        reinterpret_cast<std::uintptr_t>(gcaiDiag);
                    unsigned long uiState[12]{};
                    bool uiStateOk = true;
                    for (int field = 0; field < 12; ++field) {
                        MEMORY_BASIC_INFORMATION memory{};
                        const auto at =
                            gcaiBase + 0x6834 +
                            static_cast<std::uintptr_t>(field) * 4;
                        if (VirtualQuery(reinterpret_cast<const void*>(at),
                                         &memory,
                                         sizeof(memory)) != sizeof(memory) ||
                            memory.State != MEM_COMMIT ||
                            (memory.Protect &
                             (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
                            uiStateOk = false;
                            break;
                        }
                        uiState[field] = *reinterpret_cast<const unsigned long*>(at);
                    }
                    if (uiStateOk) {
                        std::printf("RES0E uistate id2:");
                        for (int field = 0; field < 12; ++field) {
                            std::printf(" %08lX", uiState[field]);
                        }
                        std::printf("\n");
                    } else {
                        std::printf("RES0E uistate unreadable\n");
                    }
                    std::uintptr_t tablePtr = 0;
                    MEMORY_BASIC_INFORMATION memory{};
                    const auto tablePtrAt = gcaiBase + 0x679C;
                    if (VirtualQuery(reinterpret_cast<const void*>(tablePtrAt),
                                     &memory,
                                     sizeof(memory)) == sizeof(memory) &&
                        memory.State == MEM_COMMIT &&
                        (memory.Protect &
                         (PAGE_NOACCESS | PAGE_GUARD)) == 0) {
                        tablePtr = *reinterpret_cast<const std::uintptr_t*>(
                            tablePtrAt);
                    }
                    std::printf("RES0E tablePtr=%p\n",
                                reinterpret_cast<void*>(tablePtr));
                    const auto metricsAt = tablePtr + 0x87C4;
                    if (tablePtr != 0 &&
                        VirtualQuery(reinterpret_cast<const void*>(metricsAt),
                                     &memory,
                                     sizeof(memory)) == sizeof(memory) &&
                        memory.State == MEM_COMMIT &&
                        (memory.Protect &
                         (PAGE_NOACCESS | PAGE_GUARD)) == 0) {
                        const auto* metrics =
                            reinterpret_cast<const unsigned long*>(metricsAt);
                        std::printf(
                            "RES0E engine metrics=%08lX %08lX\n",
                            metrics[0], metrics[1]);
                    } else {
                        std::printf("RES0E engine metrics unreadable\n");
                    }
                } else {
                    std::printf("RES0E GCalAppIF.dll not loaded\n");
                }
                std::fflush(stdout);
            }
            for (int stage2Sample = 0; stage2Sample < 8; ++stage2Sample) {
                PumpMessagesFor(500);
                PrintCpuState("res0e-stage2");
                std::printf(
                    "RES0E stage2 sample=%d lcd_nonzero=%zu callbacks=%lu\n",
                    stage2Sample,
                    MeasureLcdImageNonzero(),
                    g_callbackCount.load());
                std::fflush(stdout);
            }
            DumpLcdImage(runtime + L"\\lcd-image-res0e2.bin");
            DumpGcalInfoRam(runtime + L"\\gcal-info-ram-res0e2.bin");
            if (diagnosticResident0eKeys) {
                // F1-D: secuencia de teclas GCAI post-MENU con dumps por tecla.
                using DlsimGcaiKeyPress = void(__stdcall*)(int mode, int keyCode);
                const HMODULE dlsimModuleKeys = GetModuleHandleW(L"DLSim32E.dll");
                const auto keyPressKeys =
                    dlsimModuleKeys == nullptr
                        ? nullptr
                        : reinterpret_cast<DlsimGcaiKeyPress>(GetProcAddress(
                              dlsimModuleKeys, "_DLSIM_GCAIKeyPress@8"));
                std::printf("RES0EKEY keyPress=%p teclas=%zu\n",
                            keyPressKeys == nullptr
                                ? nullptr
                                : reinterpret_cast<void*>(keyPressKeys),
                            res0eKeyList.size());
                if (keyPressKeys != nullptr) {
                    int keyIdx = 0;
                    for (const unsigned long code : res0eKeyList) {
                        std::printf("RES0EKEY idx=%d down code=0x%04lX\n",
                                    keyIdx, code);
                        std::fflush(stdout);
                        keyPressKeys(1, static_cast<int>(code));
                        PumpMessagesFor(400);
                        keyPressKeys(2, 0);
                        // Pacing real-time: la animacion de apertura de
                        // Run-Matrix dura ~2s reales; 2.5s por tecla evita
                        // perder pulsaciones (leccion de la corrida v1).
                        PumpMessagesFor(2500);
                        DumpLcdImage(runtime + L"\\lcd-res0ekey-" +
                                     std::to_wstring(keyIdx) + L".bin");
                        DumpGuestMemoryWindow(
                            0x880D3400u, 0xE00,
                            runtime + L"\\hist-res0ekey-" +
                                std::to_wstring(keyIdx) + L".bin");
                        DumpGuestMemoryWindow(
                            0x88184D00u, 0x1100,
                            runtime + L"\\work-res0ekey-" +
                                std::to_wstring(keyIdx) + L".bin");
                        DumpGuestMemoryWindow(
                            0x8804F800u, 0x400,
                            runtime + L"\\disp-res0ekey-" +
                                std::to_wstring(keyIdx) + L".bin");
                        ++keyIdx;
                    }
                    // Espera de la evaluacion y captura final.
                    PumpMessagesFor(2500);
                    DumpLcdImage(runtime + L"\\lcd-res0ekey-final.bin");
                    DumpGuestMemoryWindow(0x880D3400u, 0xE00,
                                          runtime + L"\\hist-res0ekey-final.bin");
                    // RAM completa del guest para el diferencial total contra
                    // el save-state nativo (draw_result.state).
                    DumpGuestMemoryWindow(0x88000000u, 0x200000,
                                          runtime + L"\\fullram-res0ekey-final.bin");
                    DumpGuestMemoryWindow(0x88184D00u, 0x1100,
                                          runtime + L"\\work-res0ekey-final.bin");
                    DumpGuestMemoryWindow(0x8804F800u, 0x400,
                                          runtime + L"\\disp-res0ekey-final.bin");
                    // ROM 0x00800000-0x00B5FFFF (agujero 0xFF de nuestro
                    // volcado): ¿el oficial materializa datos ahi tras usar
                    // la calculadora? 14 trozos de 0x40000.
                    for (std::uint32_t romOff = 0x00800000u;
                         romOff < 0x00B60000u; romOff += 0x40000u) {
                        std::size_t chunk = 0x40000u;
                        if (romOff + chunk > 0x00B60000u) {
                            chunk = 0x00B60000u - romOff;
                        }
                        DumpGuestMemoryWindow(
                            romOff, chunk,
                            runtime + L"\\romhole-" +
                                std::to_wstring(romOff) + L".bin");
                    }
                    std::printf("RES0EKEY secuencia completa, dumps listos\n");
                } else {
                    std::printf("RES0EKEY sin GCAIKeyPress (DLSim32E no cargado)\n");
                }
            }
            wchar_t dumpName[64]{};
            std::swprintf(dumpName, 64, L"guest-%08lX-%zX.bin",
                          static_cast<unsigned long>(guestDumpStart),
                          guestDumpSize);
            DumpGuestMemoryWindow(guestDumpStart,
                                  guestDumpSize,
                                  runtime + L"\\" + dumpName);
            // Teardown limpio de las tareas creadas en res0e (C22 / Ítem 33)
            std::printf("RES0E clean teardown\n");
            for (auto it = std::rbegin(stage2Ids); it != std::rend(stage2Ids); ++it) {
                terminateResidentTask(reinterpret_cast<void*>(*it));
            }
            terminateResidentTask(reinterpret_cast<void*>(2));
            std::fflush(stdout);
        }
        PrintRequestState("after-hold");
        PrintKeyState("after-hold");
        PrintGcsimState("after-hold");
        PrintCpuState("after-hold");
        PrintCpuMemoryAtResetVector("after-hold");
        if (diagnosticDumpGuest) {
            wchar_t dumpName[64]{};
            std::swprintf(dumpName, 64, L"guest-%08lX-%zX.bin",
                         static_cast<unsigned long>(guestDumpStart),
                         guestDumpSize);
            DumpGuestMemoryWindow(guestDumpStart,
                                  guestDumpSize,
                                  runtime + L"\\" + dumpName);
        }
        PrintRelevantModules("after-hold");
        PrintLcdState();
        DumpLcdImage(runtime + L"\\lcd-image-bgra.bin");
        DumpLoadedModuleSection(
            L"LCDR61524A.dll",
            ".data",
            runtime + L"\\lcd-data-after-run.bin");
        DumpGcalInfoRam(runtime + L"\\gcal-info-ram.bin");
        InspectAndDumpCpuRom("after-hold", runtime);
        int terminateResult = -1;
        if (terminateEmulatorEx != nullptr) {
            terminateResult = terminateEmulatorEx(emulatorHandle, 0);
            std::printf("TERMINATE_EX(mode=0) result=%d callbacks=%lu\n",
                        terminateResult, g_callbackCount.load());
        }
        if (terminateResult != 0) {
            const int residentTerminateResult =
                residentCreateResult == 0
                    ? terminateResidentTask(reinterpret_cast<void*>(residentTaskId))
                    : 0;
            std::printf("RESIDENT_TERMINATE result=%d\n", residentTerminateResult);
            terminateResult = terminateEmulator(emulatorHandle);
            std::printf(
                "TERMINATE fallback result=%d callbacks=%lu\n",
                terminateResult,
                g_callbackCount.load());
        }
        exitCode = terminateResult == 0 ? 0 : 31;
    } else {
        std::printf("RESULT engine_not_created\n");
        exitCode = 30;
    }

    // Module is kept loaded in process to avoid tear-down races with background workers
    // (exact same design as official exe and cg50_backend.cpp).
    SetDllDirectoryW(nullptr);
    return exitCode;
}
