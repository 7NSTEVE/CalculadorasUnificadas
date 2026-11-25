#include <windows.h>
#include <iostream>
#include <vector>

using GcaiCreateEmulator = int(__stdcall*)(const char*, const char*, void*, void*, void**, int, void*);
using GcaiCreateResidentTask = int(__stdcall*)(void*, void*, void*);
using GcaiRequestResidentTask = int(__stdcall*)(void*, void*, int);
using DlsimCpuExecute = int(__stdcall*)(int cycles);
using DlsimGcaiUpdateDisplay = void(__stdcall*)(int mode);
using DlsimGcaiKeyPress = void(__stdcall*)(int mode, int keyCode);
using ImageAccessor = void*(__stdcall*)(int acquire);

static void NullCallback(void*, int, void*) {}

bool PatchActivation() {
    HMODULE actMod = GetModuleHandleW(L"ActivationFxMb.dll");
    if (!actMod) return false;
    uint8_t* target = reinterpret_cast<uint8_t*>(actMod) + 0x19D0;
    DWORD oldProtect = 0;
    if (VirtualProtect(target, 3, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        target[0] = 0x33;
        target[1] = 0xC0;
        target[2] = 0xC3;
        VirtualProtect(target, 3, oldProtect, &oldProtect);
        return true;
    }
    return false;
}

int main() {
    std::wstring runtimeDir = L"D:\\OneDrive\\Desktop\\Calculadoras (blanca y gris)\\Calculadora Unificada\\build\\Release\\runtime";
    std::wstring dllPath = runtimeDir + L"\\GCalAppIF.dll";
    SetCurrentDirectoryW(runtimeDir.c_str());
    SetDllDirectoryW(runtimeDir.c_str());

    HMODULE gcai = LoadLibraryW(dllPath.c_str());
    if (!gcai) {
        std::cout << "Failed to load GCalAppIF.dll: " << GetLastError() << "\n";
        return 1;
    }
    PatchActivation();

    auto createEmu = (GcaiCreateEmulator)GetProcAddress(gcai, "_GCAI_CreateEmulator@28");
    auto createRes = (GcaiCreateResidentTask)GetProcAddress(gcai, "_GCAI_CreateResidentTask@12");
    auto reqRes = (GcaiRequestResidentTask)GetProcAddress(gcai, "_GCAI_RequestResidentTask@12");

    void* hEmu = nullptr;
    std::string base = "D:\\OneDrive\\Desktop\\Calculadoras (blanca y gris)\\Calculadora Unificada\\build\\Release\\runtime\\";
    std::string proj = base + "fx-CG50 Manager PLUS Subscription\\EmulatorData\\CGM.dlp";

    int cr = createEmu(base.c_str(), proj.c_str(), (void*)NullCallback, nullptr, &hEmu, 0x51A1, nullptr);
    std::cout << "CreateEmulator: " << cr << " handle: " << hEmu << "\n";

    createRes((void*)3, (void*)NullCallback, nullptr);
    uint32_t req[64] = {5};
    reqRes((void*)3, req, 0);

    HMODULE keyMod = GetModuleHandleW(L"Key1616SwA.dll");
    HMODULE cpuMod = GetModuleHandleW(L"CPU73050.dll");
    HMODULE dlsimMod = GetModuleHandleW(L"DLSim32E.dll");
    HMODULE lcdMod = GetModuleHandleW(L"LCDR61524A.dll");

    auto cpuExecute = (DlsimCpuExecute)GetProcAddress(dlsimMod, "_DLSIM_CPUExecute@4");
    auto updateDisplay = (DlsimGcaiUpdateDisplay)GetProcAddress(dlsimMod, "_DLSIM_GCAIUpdateDisplay@4");
    auto keyPress = (DlsimGcaiKeyPress)GetProcAddress(dlsimMod, "_DLSIM_GCAIKeyPress@8");

    // Toggle reset
    if (keyMod) {
        auto fn = (void(__stdcall*)(int, int))((uintptr_t)keyMod + 0xBB14);
        fn(1, 1);
        Sleep(50);
        fn(1, 0);
        Sleep(50);
    }

    uint32_t* pcPtr = (uint32_t*)((uintptr_t)cpuMod + 0x675A18);
    std::cout << "Initial PC: 0x" << std::hex << *pcPtr << "\n";

    // Press AC/ON
    if (keyPress) {
        keyPress(1, 0x0101);
    }

    // Step CPU
    std::cout << "Stepping CPU with cpuExecute...\n";
    for (int i = 0; i < 50; ++i) {
        if (cpuExecute) {
            int res = cpuExecute(0);
        }
        if (i == 5 && keyPress) {
            keyPress(2, 0); // release AC
        }
        if (i % 5 == 0) {
            std::cout << "Step " << std::dec << i << " PC: 0x" << std::hex << *pcPtr << "\n";
        }
    }

    if (updateDisplay) updateDisplay(1);

    auto imageAccessor = *(ImageAccessor*)((uintptr_t)lcdMod + 0xD8EF0);
    if (imageAccessor) {
        void* img = imageAccessor(1);
        if (img) {
            uint16_t* pix = (uint16_t*)((uint8_t*)img + 0x87CC);
            int nonZero = 0;
            for (int p = 0; p < 396 * 224; ++p) {
                if (pix[p] != 0) nonZero++;
            }
            std::cout << "LCD Nonzero Pixels: " << std::dec << nonZero << " / " << (396 * 224) << "\n";
            imageAccessor(0);
        }
    }

    return 0;
}
