#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include "cg50_backend.h"
#include "cg50_gcal_profile.h"
#include "debug_log.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::atomic<int> g_liveGcalEmulators{0};

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
using GcaiCreateResidentTask = int(__stdcall*)(
    void* emulatorHandle,
    GcaiCallback callback,
    void* callbackContext);
using GcaiRequestResidentTask = int(__stdcall*)(
    void* emulatorHandle,
    void* requestBlock,
    int notifyCompletion);
using GcaiTerminateResidentTask = int(__stdcall*)(void* emulatorHandle);

using DlsimGcaiKeyPress = void(__stdcall*)(int mode, int keyCode);
using DlsimGcaiUpdateDisplay = void(__stdcall*)(int mode);
using DlsimCpuExecute = int(__stdcall*)(int cycles);
using DlsimCpuInitMode = void(__stdcall*)(int mode);
using DlsimCpuPowerMode = void(__stdcall*)(int mode);
using ImageAccessor = void*(__stdcall*)(int acquire);

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

DlsimGcaiUpdateDisplay g_callbackUpdateDisplay = nullptr;

int __stdcall EmulatorCallback(int eventId, void* /*p1*/, void* /*p2*/)
{
    if (eventId == 3 && g_callbackUpdateDisplay) {
        g_callbackUpdateDisplay(1);
    }
    return 0;
}

int __stdcall ResidentTaskCallback(int /*eventCode*/, void* /*payload*/, void* /*context*/)
{
    // Callback del resident task (ID 3). El host oficial pasa aqui su propio
    // despachador (FUN_40d4d0) y el probe valida con uno de solo-log. Un callback
    // NULO deja la peticion de arranque (comando 5) sin completar -> bloqueo
    // infinito en _GCAI_RequestResidentTask (espera de completacion).
    return 0;
}

std::string ToAnsi(const std::wstring& value) {
    const int required = WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1) return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_ACP, 0, value.c_str(), -1, result.data(), required, nullptr, nullptr);
    result.resize(static_cast<std::size_t>(required - 1));
    return result;
}

void PumpHostMessages(DWORD ms) {
    static bool inPump = false;
    if (inPump) return;
    inPump = true;
    const ULONGLONG end = GetTickCount64() + ms;
    while (GetTickCount64() < end) {
        MSG msg{};
        // El deadline se comprueba ENTRE mensajes: la app genera un flujo
        // continuo (WM_TIMER -> advance -> InvalidateRect -> WM_PAINT -> ...)
        // y un PeekMessage en bucle interno nunca se vaciaria.
        if (!PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            Sleep(10);
            continue;
        }
        if (msg.message == WM_QUIT) {
            // Do not swallow WM_QUIT during nested CG50 message pumping; restore it
            // so the outer wWinMain GetMessage loop exits cleanly when init yields.
            PostQuitMessage(static_cast<int>(msg.wParam));
            inPump = false;
            return;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    inPump = false;
}

bool ModuleContains(HMODULE module, std::uintptr_t rva, std::size_t bytes)
{
    if (!module) return false;
    MODULEINFO info{};
    if (!GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info))) {
        return false;
    }
    return rva + bytes <= static_cast<std::size_t>(info.SizeOfImage);
}

bool SetKeyboardResetSignal(const bool asserted, const Cg50GcalProfile& p)
{
#if defined(_M_IX86)
    const HMODULE key = GetModuleHandleW(L"Key1616SwA.dll");
    if (key == nullptr) return false;
    if (!ModuleContains(key, p.keySignalObjectRva, sizeof(unsigned char*)) ||
        !ModuleContains(key, p.keyUpdateCallbackRva, sizeof(void*)) ||
        !ModuleContains(key, p.keyFlushCallbackRva, sizeof(void*))) {
        return false;
    }
    const auto base = reinterpret_cast<std::uintptr_t>(key);
    auto* const signalObject = *reinterpret_cast<unsigned char**>(base + p.keySignalObjectRva);
    void* const updateCallback = *reinterpret_cast<void**>(base + p.keyUpdateCallbackRva);
    const auto flushCallback = *reinterpret_cast<void(__cdecl**)()>(base + p.keyFlushCallbackRva);
    if (signalObject == nullptr || updateCallback == nullptr || flushCallback == nullptr) {
        return false;
    }
    signalObject[6] = asserted ? 0x01 : 0x09;
    *reinterpret_cast<std::uint32_t*>(signalObject + 8) = asserted ? 0U : 1U;
    InvokeDriverSignalUpdate(updateCallback, signalObject, 2);
    flushCallback();
    return true;
#else
    (void)asserted;
    (void)p;
    return false;
#endif
}

std::uint64_t FileSizeBytes(const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return 0;
    }
    ULARGE_INTEGER size{};
    size.HighPart = data.nFileSizeHigh;
    size.LowPart = data.nFileSizeLow;
    return size.QuadPart;
}

bool FileExists(const std::wstring& path)
{
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring JoinPath(const std::wstring& dir, const wchar_t* name)
{
    if (dir.empty()) return name;
    if (dir.back() == L'\\' || dir.back() == L'/') return dir + name;
    return dir + L"\\" + name;
}

std::wstring ParentDir(const std::wstring& path)
{
    const auto pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return {};
    return path.substr(0, pos);
}

std::wstring ModuleDirectory()
{
    DWORD cap = 512;
    std::wstring buf;
    for (;;) {
        buf.assign(cap, L'\0');
        SetLastError(ERROR_SUCCESS);
        const DWORD len = GetModuleFileNameW(nullptr, buf.data(), cap);
        if (len == 0) return {};
        if (len < cap && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            buf.resize(len);
            return ParentDir(buf);
        }
        if (cap >= 32768) {
            buf.resize(len);
            return ParentDir(buf);
        }
        cap *= 2;
    }
}

std::wstring GetCurrentDirectoryDynamicW()
{
    const DWORD need = GetCurrentDirectoryW(0, nullptr);
    if (need == 0) return {};
    std::wstring buf(need, L'\0');
    const DWORD written = GetCurrentDirectoryW(need, buf.data());
    if (written == 0 || written >= need) return {};
    buf.resize(written);
    return buf;
}

std::wstring GetDllDirectoryDynamicW()
{
    const DWORD need = GetDllDirectoryW(0, nullptr);
    if (need == 0) return {};
    std::wstring buf(need, L'\0');
    const DWORD written = GetDllDirectoryW(need, buf.data());
    if (written == 0 || written >= need) return {};
    buf.resize(written);
    return buf;
}

std::wstring GetEnvVarDynamicW(const wchar_t* name)
{
    if (!name) return {};
    const DWORD need = GetEnvironmentVariableW(name, nullptr, 0);
    if (need == 0) return {};
    std::wstring buf(need, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, buf.data(), need);
    if (written == 0 || written >= need) return {};
    buf.resize(written);
    return buf;
}

std::wstring GetModulePathDynamicW(HMODULE mod)
{
    if (!mod) return {};
    DWORD cap = 512;
    std::wstring buf;
    for (;;) {
        buf.assign(cap, L'\0');
        SetLastError(ERROR_SUCCESS);
        const DWORD len = GetModuleFileNameW(mod, buf.data(), cap);
        if (len == 0) return {};
        if (len < cap && GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            buf.resize(len);
            return buf;
        }
        if (cap >= 32768) {
            buf.resize(len);
            return buf;
        }
        cap *= 2;
    }
}

HMODULE AcquirePersistentModule(const std::wstring& fullPath)
{
    if (fullPath.empty()) return nullptr;
    HMODULE existing = nullptr;
    // Check without incrementing PE refcount so repeated Initialize/Reset cycles never accumulate refs.
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            fullPath.c_str(),
            &existing) && existing != nullptr) {
        return existing;
    }
    const auto slash = fullPath.find_last_of(L"\\/");
    const wchar_t* fileOnly = (slash != std::wstring::npos) ? (fullPath.c_str() + slash + 1) : fullPath.c_str();
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            fileOnly,
            &existing) && existing != nullptr) {
        const std::wstring loadedPath = GetModulePathDynamicW(existing);
        if (!loadedPath.empty() && _wcsicmp(loadedPath.c_str(), fullPath.c_str()) == 0) {
            return existing;
        }
    }
    return LoadLibraryW(fullPath.c_str());
}

bool LooksLikeGcalDir(const std::wstring& dir)
{
    return FileExists(JoinPath(dir, L"GCalAppIF.dll"));
}

bool FindGcalDirectory(std::wstring& outDir)
{
    const std::wstring envDir = GetEnvVarDynamicW(L"GCAL_RUNTIME_DIR");
    if (!envDir.empty() && LooksLikeGcalDir(envDir)) {
        outDir = envDir;
        return true;
    }

    const std::wstring exeDir = ModuleDirectory();
    const std::wstring cwd = GetCurrentDirectoryDynamicW();
    const std::vector<std::wstring> candidates =
        Cg50PathDiagnostics::ResolveRuntimeSearchCandidates(exeDir, cwd);

    for (const auto& dir : candidates) {
        if (!dir.empty() && LooksLikeGcalDir(dir)) {
            outDir = dir;
            return true;
        }
    }
    return false;
}

bool IsValidExecutableCodeRva(HMODULE mod, uintptr_t rva, size_t bytes)
{
    if (!ModuleContains(mod, rva, bytes)) return false;
    const auto* base = reinterpret_cast<const uint8_t*>(mod);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    if (nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return false;
    if (rva + bytes > nt->OptionalHeader.SizeOfImage) return false;

    const auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const DWORD secStart = sec[i].VirtualAddress;
        const DWORD secSpan = (std::max)(sec[i].Misc.VirtualSize, sec[i].SizeOfRawData);
        if (rva >= secStart && rva + bytes <= static_cast<size_t>(secStart) + secSpan) {
            const DWORD chars = sec[i].Characteristics;
            return (chars & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_CNT_CODE)) != 0;
        }
    }
    return false;
}

bool PatchActivationModule(HMODULE actMod, std::wstring& outDetail) {
    if (!actMod) {
        outDetail = L"Módulo ActivationFxMb.dll no válido";
        return false;
    }

    struct PatchEntry {
        const char* name;
        uintptr_t rva;
        const uint8_t bytes[8];
        size_t len;
    };

    // Set V1 validado dinámicamente (INFORME-C 2026-10-20):
    // GetInfomation2 -> 1, IsActivationDialog2 -> 1, GetPropertyPage2 -> 1,
    // IsLicenseStatus -> 0, ActivationErrorDlg neutralizada.
    // Todos con ret N exacto para respetar el convenio stdcall y evitar desalinear la pila.
    static const PatchEntry kPatches[] = {
        { "AcvFx_GetInfomation2", 0x19D0, { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x04, 0x00 }, 8 }, // mov eax, 1; ret 4
        { "AcvFx_IsActivationDialog2", 0x1A20, { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x0C, 0x00 }, 8 }, // mov eax, 1; ret 0xC
        { "AcvFx_GetPropertyPage2", 0x1AB0, { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x04, 0x00 }, 8 }, // mov eax, 1; ret 4
        { "AcvFx_IsLicenseStatus", 0x2180, { 0x33, 0xC0, 0xC3 }, 3 }, // xor eax, eax; ret
        { "AcvFx_ActivationErrorDlg", 0x21B0, { 0x33, 0xC0, 0xC2, 0x0C, 0x00 }, 5 }, // xor eax, eax; ret 0xC
        { "AcvFx_IsActivationDialog", 0x1D70, { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x0C, 0x00 }, 8 }, // mov eax, 1; ret 0xC
        { "AcvFx_DispActivationToolMenu", 0x1F40, { 0x33, 0xC0, 0xC2, 0x04, 0x00 }, 5 }, // xor eax, eax; ret 4
        { "AcvFx_DispActivationToolActivation", 0x2010, { 0x33, 0xC0, 0xC2, 0x04, 0x00 }, 5 }, // xor eax, eax; ret 4
    };

    // Pass 1: validate PE geometry, executable code section bounds, and known prologue / pre-patched bytes.
    bool allAlreadyMatch = true;
    for (const auto& p : kPatches) {
        if (!IsValidExecutableCodeRva(actMod, p.rva, p.len)) {
            outDetail = L"RVA fuera de secci\u00F3n de c\u00F3digo ejecutable PE en " +
                std::wstring(p.name, p.name + strlen(p.name));
            return false;
        }
        const uint8_t* target = reinterpret_cast<const uint8_t*>(actMod) + p.rva;
        bool already = false;
        if (!Cg50PathDiagnostics::ValidateActivationPatchSiteBytes(target, p.len, p.bytes, p.len, already)) {
            outDetail = L"Firma binaria/pr\u00F3logo incompatible en " +
                std::wstring(p.name, p.name + strlen(p.name));
            return false;
        }
        if (!already) {
            allAlreadyMatch = false;
        }
    }

    if (allAlreadyMatch) {
        outDetail = L"Set V1 de activaci\u00F3n ya verificado en memoria (pre-parcheado en disco)";
        return true;
    }

    // Pass 2: atomic patch application after full validation.
    for (const auto& p : kPatches) {
        uint8_t* target = reinterpret_cast<uint8_t*>(actMod) + p.rva;
        if (memcmp(target, p.bytes, p.len) == 0) continue;

        DWORD oldProtect = 0;
        if (!VirtualProtect(target, p.len, PAGE_EXECUTE_READWRITE, &oldProtect)) {
            outDetail = L"VirtualProtect fall\u00F3 en " + std::wstring(p.name, p.name + strlen(p.name));
            return false;
        }
        memcpy(target, p.bytes, p.len);
        VirtualProtect(target, p.len, oldProtect, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), target, p.len);

        if (memcmp(target, p.bytes, p.len) != 0) {
            outDetail = L"Verificaci\u00F3n de bytes fall\u00F3 en " + std::wstring(p.name, p.name + strlen(p.name));
            return false;
        }
    }
    outDetail = L"Puntos de activaci\u00F3n V1 interceptados y verificados en RAM";
    return true;
}

struct EmulatorDataSeed {
    bool userDataFound = false;
    bool cgmDlmPresent = false;
    bool romdPresent = false;
    unsigned int filesCopied = 0;
    bool copyOk = true;
};

bool CopyDirectoryIfNewer(const std::wstring& src, const std::wstring& dst, unsigned int& filesCopied)
{
    CreateDirectoryW(dst.c_str(), nullptr);
    WIN32_FIND_DATAW fd{};
    const HANDLE hFind = FindFirstFileW((src + L"\\*").c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        const std::wstring srcEntry = src + L"\\" + fd.cFileName;
        const std::wstring dstEntry = dst + L"\\" + fd.cFileName;
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) continue;
            if (!CopyDirectoryIfNewer(srcEntry, dstEntry, filesCopied)) ok = false;
            continue;
        }
        bool copy = true;
        WIN32_FILE_ATTRIBUTE_DATA dstInfo{};
        if (GetFileAttributesExW(dstEntry.c_str(), GetFileExInfoStandard, &dstInfo)) {
            copy = CompareFileTime(&fd.ftLastWriteTime, &dstInfo.ftLastWriteTime) > 0;
        }
        if (copy) {
            if (CopyFileW(srcEntry.c_str(), dstEntry.c_str(), FALSE)) ++filesCopied;
            else ok = false;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return ok;
}

EmulatorDataSeed SeedUserEmulatorData(const std::wstring& runtimeDir)
{
    EmulatorDataSeed seed;
    const std::wstring appData = GetEnvVarDynamicW(L"APPDATA");
    if (appData.empty()) return seed;

    const std::wstring userData =
        appData + L"\\CASIO\\fx-CG50 Manager PLUS Subscription\\EmulatorData";
    if (GetFileAttributesW(userData.c_str()) == INVALID_FILE_ATTRIBUTES) return seed;

    seed.userDataFound = true;
    const std::wstring targetData =
        runtimeDir + L"\\fx-CG50 Manager PLUS Subscription\\EmulatorData";
    seed.copyOk = CopyDirectoryIfNewer(userData, targetData, seed.filesCopied);
    seed.cgmDlmPresent = FileExists(JoinPath(targetData, L"CGM.dlm"));
    seed.romdPresent = FileExists(targetData + L"\\INIT\\CasioROMD.mem");
    return seed;
}

} // namespace

struct Cg50Backend::Impl {
    const Cg50GcalProfile profile = kCg50GcalProfile;
    HMODULE gcaiModule = nullptr;
    void* emulatorHandle = nullptr;
    GcaiCreateResidentTask createResidentTask = nullptr;
    GcaiRequestResidentTask requestResidentTask = nullptr;
    GcaiTerminateEmulator terminateEmulator = nullptr;
    GcaiTerminateEmulatorEx terminateEmulatorEx = nullptr;
    GcaiTerminateResidentTask terminateResidentTask = nullptr;
    std::vector<std::uintptr_t> registeredTaskIds;
    DlsimGcaiKeyPress keyPress = nullptr;
    DlsimGcaiUpdateDisplay updateDisplay = nullptr;
    DlsimCpuExecute cpuExecute = nullptr;
    DlsimCpuInitMode cpuInitMode = nullptr;
    DlsimCpuPowerMode cpuPowerMode = nullptr;
    ImageAccessor lcdAccessor = nullptr;
    std::wstring dllDirectory;
    std::wstring savedDllDirectory;
    std::wstring savedCurrentDirectory;
    bool dllDirectoryChanged = false;
    bool dllsWereFound = false;
    bool systemRomPresent = false;
    bool userRomPresent = false;

    bool ownsLiveEmulator = false;

    void RestoreDllDirectory()
    {
        if (!dllDirectoryChanged) return;
        if (savedDllDirectory.empty()) {
            SetDllDirectoryW(nullptr);
        } else {
            SetDllDirectoryW(savedDllDirectory.c_str());
        }
        if (!savedCurrentDirectory.empty()) {
            SetCurrentDirectoryW(savedCurrentDirectory.c_str());
        }
        savedDllDirectory.clear();
        savedCurrentDirectory.clear();
        dllDirectoryChanged = false;
    }
};

Cg50Backend::Cg50Backend() : impl_(std::make_unique<Impl>()) {}

Cg50Backend::~Cg50Backend()
{
    UnloadHost();
}

void Cg50Backend::UnloadHost()
{
    if (!impl_) return;
    if (impl_->emulatorHandle) {
        // Terminar slots secundarios (0x0E, 0x10, 0x12, etc.), PERO NO el slot 3 (canal principal)
        // porque TerminateEmulatorEx(handle, 0) necesita slot 3 activo para señalizar comando 6 a GCSim.
        if (impl_->terminateResidentTask && !impl_->registeredTaskIds.empty()) {
            for (auto it = impl_->registeredTaskIds.rbegin();
                 it != impl_->registeredTaskIds.rend(); ++it) {
                if (*it != impl_->profile.residentTaskId) {
                    impl_->terminateResidentTask(reinterpret_cast<void*>(*it));
                }
            }
        }
        impl_->registeredTaskIds.clear();
        if (impl_->terminateEmulatorEx) {
            impl_->terminateEmulatorEx(impl_->emulatorHandle, 0);
        } else if (impl_->terminateEmulator) {
            impl_->terminateEmulator(impl_->emulatorHandle);
        }
        impl_->emulatorHandle = nullptr;
        if (impl_->ownsLiveEmulator) {
            g_liveGcalEmulators.fetch_sub(1);
            impl_->ownsLiveEmulator = false;
        }
    }
    if (impl_->gcaiModule) {
        // Module is kept loaded in process to avoid thread tear-down races with DLSim32E background workers
        impl_->gcaiModule = nullptr;
    }
    impl_->createResidentTask = nullptr;
    impl_->requestResidentTask = nullptr;
    impl_->terminateEmulator = nullptr;
    impl_->terminateEmulatorEx = nullptr;
    impl_->terminateResidentTask = nullptr;
    impl_->keyPress = nullptr;
    impl_->updateDisplay = nullptr;
    impl_->cpuExecute = nullptr;
    impl_->cpuInitMode = nullptr;
    impl_->cpuPowerMode = nullptr;
    impl_->lcdAccessor = nullptr;
    g_callbackUpdateDisplay = nullptr;
    impl_->RestoreDllDirectory();
    framebufferCopy_.clear();
    fbWidth_ = fbHeight_ = fbStride_ = 0;
    impl_->systemRomPresent = false;
    impl_->userRomPresent = false;
    engine_ = Engine::None;
}

bool Cg50Backend::CaptureFramebuffer()
{
    const auto& p = impl_->profile;
    if (!impl_->lcdAccessor) return false;

    void* const image = impl_->lcdAccessor(1);
    if (!image) {
        impl_->lcdAccessor(0);
        return false;
    }

    const auto* const imageBytes = static_cast<const unsigned char*>(image);
    const std::size_t surfaceBytes = Cg50SurfaceBytes(p);
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool copySuccess = false;

    __try {
        const auto contentWidth = *reinterpret_cast<const std::uint32_t*>(
            imageBytes + p.lcdContentWidthOffset);
        const auto contentHeight = *reinterpret_cast<const std::uint32_t*>(
            imageBytes + p.lcdContentHeightOffset);
        width = contentWidth != 0 ? contentWidth : p.lcdExpectedContentWidth;
        height = contentHeight != 0 ? contentHeight : p.lcdExpectedContentHeight;

        const std::size_t lastRowOffset =
            (height > 0) ? (static_cast<std::size_t>(height - 1u) * p.lcdSurfaceWidth * p.lcdBytesPerPixel) : 0u;
        const std::size_t rowBytes = static_cast<std::size_t>(width) * p.lcdBytesPerPixel;
        const std::size_t pixelSpanBytes =
            (width > 0 && height > 0)
                ? (static_cast<std::size_t>(height - 1u) * p.lcdSurfaceWidth + width) * sizeof(std::uint32_t)
                : 0u;
        const bool withinSurface =
            (width > 0 && height > 0 &&
             width <= p.lcdSurfaceWidth && height <= p.lcdSurfaceHeight &&
             p.lcdPixelOffset <= surfaceBytes &&
             lastRowOffset <= surfaceBytes - p.lcdPixelOffset &&
             rowBytes <= (surfaceBytes - p.lcdPixelOffset) - lastRowOffset &&
             pixelSpanBytes <= surfaceBytes - p.lcdPixelOffset);

        if (withinSurface) {
            framebufferCopy_.assign(static_cast<std::size_t>(width) * height, 0);
            const auto* src = reinterpret_cast<const std::uint32_t*>(imageBytes + p.lcdPixelOffset);
            for (std::uint32_t y = 0; y < height; ++y) {
                std::memcpy(
                    framebufferCopy_.data() + static_cast<std::size_t>(y) * width,
                    src + static_cast<std::size_t>(y) * p.lcdSurfaceWidth,
                    static_cast<std::size_t>(width) * sizeof(std::uint32_t));
            }
            copySuccess = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        copySuccess = false;
    }
    impl_->lcdAccessor(0);

    if (!copySuccess) return false;

    fbWidth_ = width;
    fbHeight_ = height;
    fbStride_ = width;
    return true;
}

bool Cg50Backend::Initialize(const std::uint8_t* /*firmware*/, const std::size_t /*firmwareSize*/)
{
    UnloadHost();
    stages_ = StageStatus{};
    state_ = State::Unavailable;
    diagnostic_ = L"Buscando runtime GCal (GCalAppIF.dll)";

    if (g_liveGcalEmulators.load() > 0) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 0] Ya existe una instancia GCal activa en este proceso";
        return false;
    }

    std::wstring targetDir;
    if (!FindGcalDirectory(targetDir)) {
        state_ = State::Unavailable;
        diagnostic_ = L"[Etapa 0] No se encontró GCalAppIF.dll (exe, runtime\\, ni instalación CASIO)";
        return false;
    }
    log_msg("CG50 etapa 0 OK: runtime hallado");
    impl_->dllsWereFound = true;
    impl_->dllDirectory = targetDir;

    impl_->savedDllDirectory = GetDllDirectoryDynamicW();
    impl_->savedCurrentDirectory = GetCurrentDirectoryDynamicW();
    SetCurrentDirectoryW(targetDir.c_str());
    SetDllDirectoryW(targetDir.c_str());
    impl_->dllDirectoryChanged = true;

    const auto& p = impl_->profile;
    const std::wstring dllPath = JoinPath(targetDir, L"GCalAppIF.dll");
    const std::wstring lcdPath = JoinPath(targetDir, L"LCDR61524A.dll");
    const std::wstring cpuPath = JoinPath(targetDir, L"CPU73050.dll");
    if (FileSizeBytes(lcdPath) != 0 && FileSizeBytes(lcdPath) < p.lcdDllMinBytes) {
        state_ = State::Faulted;
        diagnostic_ = L"[Perfil] LCDR61524A.dll es demasiado pequeña para los RVA conocidos";
        UnloadHost();
        return false;
    }
    if (FileSizeBytes(cpuPath) != 0 && FileSizeBytes(cpuPath) < p.cpuDllMinBytes) {
        state_ = State::Faulted;
        diagnostic_ = L"[Perfil] CPU73050.dll es demasiado pequeña para los RVA conocidos";
        UnloadHost();
        return false;
    }

    std::wstring actPath = JoinPath(targetDir, L"ActivationFxMb.dll");
    if (!FileExists(actPath)) {
        actPath = JoinPath(
            LR"(C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series)",
            L"ActivationFxMb.dll");
    }
    HMODULE hAct = AcquirePersistentModule(actPath);
    if (!hAct) {
        DWORD err = GetLastError();
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 1/11 FAILED] Error al precargar ActivationFxMb.dll: " + std::to_wstring(err);
        UnloadHost();
        return false;
    }
    stages_.stage1_activationLoaded = true;
    log_msg("CG50 etapa 1 OK: ActivationFxMb cargada");
    stages_.currentStage = 1;

    std::wstring patchDetail;
    if (!PatchActivationModule(hAct, patchDetail)) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 2/11 FAILED] " + patchDetail;
        UnloadHost();
        return false;
    }
    stages_.stage2_activationPatched = true;
    log_msg("CG50 etapa 2 OK: activacion parcheada");
    stages_.currentStage = 2;

    impl_->gcaiModule = AcquirePersistentModule(dllPath);
    if (!impl_->gcaiModule) {
        DWORD err = GetLastError();
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 3/11 FAILED] Error al cargar GCalAppIF.dll: " + std::to_wstring(err);
        UnloadHost();
        return false;
    }
    stages_.stage3_gcalLoaded = true;
    log_msg("CG50 etapa 3 OK: GCalAppIF cargada");
    stages_.currentStage = 3;

    const auto createEmulator = reinterpret_cast<GcaiCreateEmulator>(
        GetProcAddress(impl_->gcaiModule, "_GCAI_CreateEmulator@28"));
    impl_->terminateEmulator = reinterpret_cast<GcaiTerminateEmulator>(
        GetProcAddress(impl_->gcaiModule, "_GCAI_TerminateEmulator@4"));
    impl_->terminateEmulatorEx = reinterpret_cast<GcaiTerminateEmulatorEx>(
        GetProcAddress(impl_->gcaiModule, "_GCAI_TerminateEmulatorEx@8"));
    impl_->createResidentTask = reinterpret_cast<GcaiCreateResidentTask>(
        GetProcAddress(impl_->gcaiModule, "_GCAI_CreateResidentTask@12"));
    const auto createResidentTask = impl_->createResidentTask;
    impl_->requestResidentTask = reinterpret_cast<GcaiRequestResidentTask>(
        GetProcAddress(impl_->gcaiModule, "_GCAI_RequestResidentTask@12"));
    impl_->terminateResidentTask = reinterpret_cast<GcaiTerminateResidentTask>(
        GetProcAddress(impl_->gcaiModule, "_GCAI_TerminateResidentTask@4"));

    if (!createEmulator || !impl_->terminateEmulator || !createResidentTask ||
        !impl_->requestResidentTask || !impl_->terminateResidentTask) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 4/11 FAILED] Faltan exportaciones de GCalAppIF";
        UnloadHost();
        return false;
    }
    stages_.stage4_exportsResolved = true;
    log_msg("CG50 etapa 4 OK: exports resueltos");
    stages_.currentStage = 4;

    const EmulatorDataSeed seed = SeedUserEmulatorData(targetDir);
    if (!seed.userDataFound) {
        log_msg("CG50 etapa 0b: sin EmulatorData de usuario en %APPDATA%; plantilla del runtime en uso (LCD puede quedar en negro)");
    } else {
        if (!seed.copyOk) {
            log_msg("CG50 etapa 0b: directorio runtime de solo lectura; no se pudo copiar EmulatorData");
        }
        char seedBuf[192]{};
        wsprintfA(seedBuf,
                  "CG50 etapa 0b: estado del usuario aplicado (copiados=%u cgm.dlm=%u romd=%u copyOk=%u)",
                  seed.filesCopied,
                  seed.cgmDlmPresent ? 1u : 0u,
                  seed.romdPresent ? 1u : 0u,
                  seed.copyOk ? 1u : 0u);
        log_msg(seedBuf);
    }

    const std::wstring emulatorData = targetDir + L"\\fx-CG50 Manager PLUS Subscription\\EmulatorData";
    // A template without the ROM payload otherwise reaches Ready with a black LCD.
    // Validate after optional user-data seeding and before creating the resident CPU.
    const wchar_t* requiredPayloads[] = {L"CGM.dlm", L"INIT\\CasioROMD.mem"};
    for (const auto* payload : requiredPayloads) {
        const std::wstring file = emulatorData + L"\\" + payload;
        if (!FileExists(file)) {
            state_ = State::Faulted;
            diagnostic_ = L"Runtime CG50 incompleto: falta " + file;
            log_msg("CG50: incomplete runtime payload; emulator was not created");
            UnloadHost();
            return false;
        }
    }
    impl_->systemRomPresent = FileExists(emulatorData + L"\\CGM.dlm");
    impl_->userRomPresent = FileExists(emulatorData + L"\\INIT\\CasioROMD.mem");

    std::wstring projectPath = emulatorData + L"\\CGM.dlp";
    if (!FileExists(projectPath)) {
        projectPath = JoinPath(targetDir, L"CGM.dlp");
    }

    std::string baseAnsi = ToAnsi(targetDir + L"\\");
    std::string projectAnsi = ToAnsi(projectPath);

    HWND hTargetWnd = GetForegroundWindow();
    if (!hTargetWnd) hTargetWnd = GetDesktopWindow();

    int createResult = createEmulator(
        baseAnsi.c_str(),
        projectAnsi.c_str(),
        EmulatorCallback,
        hTargetWnd,
        &impl_->emulatorHandle,
        p.instanceId,
        nullptr);

    if (createResult != 0 || !impl_->emulatorHandle) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 5/11 FAILED] _GCAI_CreateEmulator retornó código " +
                      std::to_wstring(createResult);
        UnloadHost();
        return false;
    }
    impl_->ownsLiveEmulator = true;
    g_liveGcalEmulators.fetch_add(1);
    stages_.stage5_createEmulatorOk = true;
    log_msg("CG50 etapa 5 OK: emulador creado");
    stages_.currentStage = 5;

    int resTaskResult = createResidentTask(
        reinterpret_cast<void*>(p.residentTaskId),
        ResidentTaskCallback,
        reinterpret_cast<void*>(0x52455349u)); // 'RESI' (mismo contexto que el probe)
    if (resTaskResult != 0) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 6/11 FAILED] _GCAI_CreateResidentTask retornó código " +
                      std::to_wstring(resTaskResult);
        UnloadHost();
        return false;
    }
    impl_->registeredTaskIds.push_back(p.residentTaskId);
    stages_.stage6_createResidentTaskOk = true;
    log_msg("CG50 etapa 6 OK: resident task creado");
    stages_.currentStage = 6;

    // Slots adicionales del host oficial (verificado en los call sites del exe
    // original): RequestResidentTask(0x10,{0},0)=power-on (RVA 0xD365) y
    // RequestResidentTask(0x12,{0},0)=reset (RVA 0xD3E5). Sin crear estos
    // slots, el despachador 0x34A0 de GCalAppIF no halla la tarea y devuelve 2
    // sin enviar nada (fallo silencioso; ver hallazgo 2026-10-16).
    for (const std::uintptr_t taskId : {p.powerCommandId, p.resetCommandId}) {
        const int extraResult = createResidentTask(
            reinterpret_cast<void*>(taskId),
            ResidentTaskCallback,
            reinterpret_cast<void*>(0x52455349u));
        char eb[128]{};
        snprintf(eb, 128, "CG50 etapa 6: slot extra 0x%zX result=%d",
                 static_cast<size_t>(taskId), extraResult);
        log_msg(eb);
        if (extraResult == 0) {
            impl_->registeredTaskIds.push_back(taskId);
        }
        // Un resultado != 0 NO es fatal: el emulador sigue arrancando por el
        // slot 3 y el power-on degradara a keyPress directo (comportamiento
        // previo al fix). Queda registrado en calc_debug.log.
    }

    // F1.5 (2026-10-16): tarea residente 0xE del host oficial. El OS del CG50
    // queda en el bucle 0x80138328-36 reintentando un servicio del host
    // (jsr a la tabla RAM 0x8041AA9C con r4=0xE, r5=0xA) hasta que la tarea
    // 0xE responde. El exe oficial la crea al arrancar con
    // CreateResidentTask(0xe, despachador, objeto). Verificado en el probe
    // (modo res0e): con este slot el OS sale del bucle y dibuja el MAIN MENU
    // (OCR sobre el LCD, 2026-10-16); sin el, la app queda en pantalla blanca
    // + spinner (bucle; captura GUI pre-fix 2026-10-16).
    constexpr std::uintptr_t checkTaskId = 0xE;
    {
        const int checkCreateResult = createResidentTask(
            reinterpret_cast<void*>(checkTaskId),
            ResidentTaskCallback,
            reinterpret_cast<void*>(0x54534B30u)); // 'TSK0' (igual que el probe res0e)
        char checkBuf[128]{};
        snprintf(checkBuf, 128, "CG50 etapa 6: slot check 0x%zX result=%d",
                 static_cast<size_t>(checkTaskId), checkCreateResult);
        log_msg(checkBuf);
        if (checkCreateResult == 0) {
            impl_->registeredTaskIds.push_back(checkTaskId);
        }
    }

    std::array<std::uint32_t, 64> startRequest{};
    startRequest[0] = p.startCommand;
    log_msg("CG50 etapa 7: pre-pump 750ms");
    PumpHostMessages(750);
    int requestResult = 5;
    unsigned long requestAttempts = 0;
    for (; requestAttempts < 20 && requestResult != 0; ++requestAttempts) {
        char rb[96]{};
        snprintf(rb, 96, "CG50 etapa 7: request intento %lu ...", requestAttempts);
        log_msg(rb);
        requestResult = impl_->requestResidentTask(
            reinterpret_cast<void*>(p.residentTaskId),
            startRequest.data(),
            1);
        snprintf(rb, 96, "CG50 etapa 7: request result=%d", requestResult);
        log_msg(rb);
        if (requestResult != 0) {
            PumpHostMessages(100);
        }
    }
    if (requestResult != 0 && requestResult != 5) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 7/11 FAILED] comando 5 resultó " +
                      std::to_wstring(requestResult) + L" tras " +
                      std::to_wstring(requestAttempts) + L" intentos";
        UnloadHost();
        return false;
    }
    PumpHostMessages(750);

    stages_.stage7_requestResidentTaskOk = true;
    log_msg("CG50 etapa 7 OK: comando start enviado");
    stages_.currentStage = 7;

    HMODULE dlsimModule = GetModuleHandleW(L"DLSim32E.dll");
    if (!dlsimModule) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 8/11 FAILED] DLSim32E.dll no está cargada";
        UnloadHost();
        return false;
    }
    impl_->keyPress = reinterpret_cast<DlsimGcaiKeyPress>(
        GetProcAddress(dlsimModule, "_DLSIM_GCAIKeyPress@8"));
    impl_->updateDisplay = reinterpret_cast<DlsimGcaiUpdateDisplay>(
        GetProcAddress(dlsimModule, "_DLSIM_GCAIUpdateDisplay@4"));
    impl_->cpuExecute = reinterpret_cast<DlsimCpuExecute>(
        GetProcAddress(dlsimModule, "_DLSIM_CPUExecute@4"));
    impl_->cpuInitMode = reinterpret_cast<DlsimCpuInitMode>(
        GetProcAddress(dlsimModule, "_DLSIM_CPUInitMode@4"));
    impl_->cpuPowerMode = reinterpret_cast<DlsimCpuPowerMode>(
        GetProcAddress(dlsimModule, "_DLSIM_CPUPowerMode@4"));

    if (!impl_->keyPress || !impl_->updateDisplay) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 8/11 FAILED] Faltan exportaciones de DLSim32E";
        UnloadHost();
        return false;
    }
    stages_.stage8_dlsimActive = true;
    log_msg("CG50 etapa 8 OK: DLSim32E activa");
    stages_.currentStage = 8;

    HMODULE lcdModule = GetModuleHandleW(L"LCDR61524A.dll");
    if (!lcdModule) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 9/11 FAILED] LCDR61524A.dll no está cargada";
        UnloadHost();
        return false;
    }

    auto lcdBase = reinterpret_cast<uintptr_t>(lcdModule);
    for (int waitIdx = 0; waitIdx < 10 && !impl_->lcdAccessor; ++waitIdx) {
        if (ModuleContains(lcdModule, p.lcdImageAccessorRva, sizeof(ImageAccessor))) {
            impl_->lcdAccessor = *reinterpret_cast<ImageAccessor const*>(lcdBase + p.lcdImageAccessorRva);
        }
        if (!impl_->lcdAccessor) {
            PumpHostMessages(100);
        }
    }
    if (!impl_->lcdAccessor) {
        state_ = State::Faulted;
        diagnostic_ = L"[Etapa 9/11 FAILED] accessor LCD nulo";
        UnloadHost();
        return false;
    }
    CaptureFramebuffer();
    stages_.stage9_lcdActive = true;
    log_msg("CG50 etapa 9 OK: LCD accessor activo");
    stages_.currentStage = 9;

    // Send reset pulse to boot SH7305 CPU emulator out of initial standby
    SetKeyboardResetSignal(true, p);
    PumpHostMessages(350);
    SetKeyboardResetSignal(false, p);
    PumpHostMessages(500);

    // Pulse AC/ON key (matrix code 0x0101) to power on LCD
    if (impl_->keyPress) {
        impl_->keyPress(1, 0x0101);
        PumpHostMessages(800);
        impl_->keyPress(2, 0);
        PumpHostMessages(200);
    }

    if (impl_->updateDisplay) impl_->updateDisplay(1);
    CaptureFramebuffer();
    stages_.stage10_cpuExecuting = true;
    log_msg("CG50 etapa 10 OK");
    stages_.currentStage = 10;

    stages_.stage11_keyInjectionActive = true;
    stages_.currentStage = 11;

    impl_->RestoreDllDirectory();
    state_ = State::Ready;
    engine_ = Engine::GCal;
    diagnostic_ = L"GCal listo (framebuffer BGRA copiado; teclas 16-bit)";
    return true;
}

void Cg50Backend::Reset()
{
    if (impl_ && impl_->requestResidentTask && state_ == State::Ready) {
        // Reset oficial: RequestResidentTask(slot 0x12, {0}, 0) — exe oficial
        // RVA 0xD3E5. Antes se pasaba resetCommandId como taskId de un payload
        // vacio: el slot 0x12 no existia y el despachador devolvia 2 sin hacer
        // nada (fallo silencioso; hallazgo 2026-10-16).
        std::array<std::uint32_t, 64> resetReq{};
        const int resetResult = impl_->requestResidentTask(
            reinterpret_cast<void*>(impl_->profile.resetCommandId),
            resetReq.data(),
            0);
        char rb[96]{};
        snprintf(rb, 96, "CG50 reset (slot 0x12) result=%d", resetResult);
        log_msg(rb);
        PumpHostMessages(50);
        if (impl_->updateDisplay) impl_->updateDisplay(1);
    }
    UnloadHost();
    stages_ = StageStatus{};
    state_ = State::Unavailable;
    diagnostic_.clear();
}

void Cg50Backend::Step(const std::uint32_t elapsedMicroseconds)
{
    (void)elapsedMicroseconds;
    if (state_ != State::Ready || !impl_) return;

    // La CPU SH7305 corre en el RESIDENT TASK creado por _GCAI_CreateResidentTask
    // (comando 5 = start). El host oficial NUNCA llama _DLSIM_CPUExecute: hacerlo
    // desde otro hilo ejecuta el dispatcher de interrupciones de CPU73050 sin el
    // contexto de registros del hilo propietario (EDI sin inicializar -> 0xC0000005
    // en CPU73050.dll+0x26072, verificado contra el desensamblado y el exe original).
    // Aqui solo nos aseguramos de que la simulacion no quede pausada.
    const auto& p = impl_->profile;
    HMODULE gcSimMod = GetModuleHandleW(L"CASIO GCSim.dll");
    if (gcSimMod &&
        ModuleContains(gcSimMod, p.gcsimPauseRva, sizeof(std::uint32_t)) &&
        ModuleContains(gcSimMod, p.gcsimRunRva, sizeof(std::uint32_t))) {
        auto pauseFlag = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uintptr_t>(gcSimMod) + p.gcsimPauseRva);
        auto runFlag = reinterpret_cast<uint32_t*>(
            reinterpret_cast<uintptr_t>(gcSimMod) + p.gcsimRunRva);
        if (*pauseFlag != 0) *pauseFlag = 0;
        if (*runFlag != 1) *runFlag = 1;
    }

    PumpHostMessages(2);
    if (impl_->updateDisplay) {
        impl_->updateDisplay(1);
        stages_.frameCount++;
    }
    CaptureFramebuffer();
}

bool Cg50Backend::KeyDown(const std::uint16_t matrixCode)
{
    if (state_ != State::Ready || !impl_ || !impl_->keyPress) return false;
    if (matrixCode == 0x0101 && impl_->requestResidentTask) {
        // Power-on oficial: RequestResidentTask(slot 0x10, {0}, 0) — exe
        // original RVA 0xD365. Hallazgo 2026-10-16: el primer argumento es el
        // SLOT/TAREA (no un "comando" del payload) y el payload correcto es un
        // unico dword en cero; {1,1,1} al slot 3 (comando 1) no enciende la
        // CPU (probe powerfirst: 10 s con LCD=0 y PC=0xA0000000).
        std::array<std::uint32_t, 64> powerReq{};
        const int powerResult = impl_->requestResidentTask(
            reinterpret_cast<void*>(impl_->profile.powerCommandId),
            powerReq.data(),
            0);
        char pb[96]{};
        snprintf(pb, 96, "CG50 power-on (slot 0x10) result=%d", powerResult);
        log_msg(pb);
    }

    // Sin _DLSIM_CPUExecute manual: el resident task es el dueno del contexto SH7305.
    impl_->keyPress(1, static_cast<int>(matrixCode));
    PumpHostMessages(8);
    if (impl_->updateDisplay) impl_->updateDisplay(1);
    CaptureFramebuffer();
    return true;
}

void Cg50Backend::KeyUp(const std::uint16_t matrixCode)
{
    if (state_ != State::Ready || !impl_ || !impl_->keyPress) return;
    // The host's all-keys-up message uses key code zero (as during power-on).
    // Reusing matrixCode leaves repeated presses latched: 1000 becomes 10.
    // Independently verified with 1000, 777, log(1000) and two DOWN presses.
    (void)matrixCode;
    impl_->keyPress(2, 0);
    PumpHostMessages(8);
    if (impl_->updateDisplay) impl_->updateDisplay(1);
    CaptureFramebuffer();
}

bool Cg50Backend::KeyStroke(const std::uint16_t matrixCode, const std::uint32_t holdMs)
{
    if (!KeyDown(matrixCode)) return false;
    Sleep(holdMs);
    KeyUp(matrixCode);
    return true;
}

Cg50Backend::FramebufferView Cg50Backend::Framebuffer()
{
    if (state_ != State::Ready) return {};
    if (framebufferCopy_.empty()) {
        CaptureFramebuffer();
    }
    if (framebufferCopy_.empty()) return {};
    return {
        framebufferCopy_.data(),
        fbWidth_,
        fbHeight_,
        fbStride_,
        PixelFormat::Bgra32
    };
}

// Stubs del build GCal (CG50_ENGINE != native): el save-state y la
// persistencia de UserROM solo existen en el backend RomNative
// (third_party/rom-native-reference/rama-cg50_backend.cpp).
bool Cg50Backend::SaveStateToFile(const wchar_t* /*path*/)
{
    if (state_ == State::Ready) {
        diagnostic_ = L"SaveStateToFile no soportado en backend GCal (requiere RomNative)";
    }
    return false;
}

bool Cg50Backend::LoadStateFromFile(const wchar_t* /*path*/)
{
    if (state_ == State::Ready) {
        diagnostic_ = L"LoadStateFromFile no soportado en backend GCal (requiere RomNative)";
    }
    return false;
}

bool Cg50Backend::PersistUserRom()
{
    if (state_ == State::Ready) {
        diagnostic_ = L"PersistUserRom no soportado en backend GCal (requiere RomNative)";
    }
    return false;
}

bool Cg50Backend::HasPersistedUserRom() const
{
    return false;
}

bool Cg50Backend::SetEngineMode(EngineMode mode)
{
    engineMode_ = mode;
    return state_ == State::Ready;
}

bool Cg50Backend::IsOfficialHelperAvailable() const
{
    return true;
}

Cg50Backend::FramebufferView Cg50Backend::NativeFramebuffer()
{
    return {};
}

Cg50Backend::FramebufferView Cg50Backend::OfficialFramebuffer()
{
    return Framebuffer();
}

Cg50Backend::LiveCompareMetrics Cg50Backend::ComputeLiveComparison(
    std::vector<std::uint32_t>* outDiffHeatmapBgra)
{
    if (outDiffHeatmapBgra) outDiffHeatmapBgra->clear();
    LiveCompareMetrics m{};
    m.officialReady = (state_ == State::Ready);
    m.officialDiag = diagnostic_;
    return m;
}

bool Cg50Backend::InitializeFromFile(const wchar_t* firmwarePath)
{
    if (firmwarePath && *firmwarePath && !FileExists(firmwarePath)) {
        UnloadHost();
        stages_ = StageStatus{};
        state_ = State::Faulted;
        diagnostic_ = L"Archivo de firmware no encontrado: " + std::wstring(firmwarePath);
        return false;
    }
    return Initialize(nullptr, 0);
}

Cg50Backend::RuntimeStatus Cg50Backend::GetRuntimeStatus() const noexcept
{
    RuntimeStatus out{};
    if (state_ == State::Ready) {
        out.cpuState = CpuState::Running;
        out.systemRomLoaded = impl_ && impl_->systemRomPresent;
        out.userRomLoaded = impl_ && impl_->userRomPresent;
        out.framebufferAvailable = !framebufferCopy_.empty();
        out.instructions = stages_.frameCount;
    }
    return out;
}

std::wstring Cg50Backend::StatusLabel() const
{
    if (state_ == State::Ready) {
        return L"GCal listo";
    }
    if (state_ == State::Faulted) {
        return L"GCal error";
    }
    return L"GCal no disponible";
}

namespace Cg50PathDiagnostics {

std::vector<std::wstring> ResolveRuntimeSearchCandidates(
    const std::wstring& exeDir,
    const std::wstring& cwd)
{
    return {
        exeDir,
        JoinPath(exeDir, L"runtime"),
        JoinPath(ParentDir(exeDir), L"runtime"),
        JoinPath(ParentDir(ParentDir(exeDir)), L"runtime"),
        JoinPath(ParentDir(ParentDir(exeDir)), LR"(private_local\official_casio_runtime)"),
        JoinPath(ParentDir(ParentDir(ParentDir(exeDir))), LR"(private_local\official_casio_runtime)"),
        JoinPath(exeDir, LR"(build\Release\runtime)"),
        cwd,
        JoinPath(cwd, L"runtime"),
        JoinPath(cwd, LR"(private_local\official_casio_runtime)"),
        JoinPath(cwd, LR"(Release\runtime)"),
        LR"(C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series)",
    };
}

bool ValidateActivationPatchSiteBytes(
    const std::uint8_t* mem,
    std::size_t avail,
    const std::uint8_t* expectedPatched,
    std::size_t len,
    bool& outAlreadyPatched)
{
    outAlreadyPatched = false;
    if (!mem || !expectedPatched || len == 0 || avail < len) return false;
    if (std::memcmp(mem, expectedPatched, len) == 0) {
        outAlreadyPatched = true;
        return true;
    }
    // Original Casio x86 function entry prologues observed on ActivationFxMb.dll:
    //   55 8B EC ...     (push ebp; mov ebp, esp)
    //   8B FF 55 8B EC   (hotpatchable mov edi, edi; push ebp; mov ebp, esp)
    //   6A ?? 68 ...     (SEH prologue push imm8; push imm32)
    //   83 EC ??         (sub esp, imm8)
    //   33 C0 C3 / B8    (simple constant stub)
    if (len >= 3 && mem[0] == 0x55 && mem[1] == 0x8B && mem[2] == 0xEC) return true;
    if (len >= 5 && mem[0] == 0x8B && mem[1] == 0xFF && mem[2] == 0x55 && mem[3] == 0x8B && mem[4] == 0xEC) return true;
    if (len >= 3 && mem[0] == 0x6A && mem[2] == 0x68) return true;
    if (len >= 3 && mem[0] == 0x83 && mem[1] == 0xEC) return true;
    if (len >= 3 && mem[0] == 0x33 && mem[1] == 0xC0) return true;
    if (len >= 3 && mem[0] == 0xB8) return true;
    return false;
}

} // namespace Cg50PathDiagnostics
