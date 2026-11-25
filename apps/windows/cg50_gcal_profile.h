#pragma once

#include <cstddef>
#include <cstdint>

// LCD/DLL layout for the locally observed fx-CG Manager PLUS Win32 host.
// RVAs are only used after SizeOfImage / export checks reject a mismatched
// binary.
struct Cg50GcalProfile {
    const wchar_t* name = L"fx-CG Manager PLUS (Win32, LCDR61524A)";

    std::uint32_t instanceId = 0x51A1;
    std::uintptr_t residentTaskId = 3;   // Canal de comandos principal / comunicación host
    std::uint32_t startCommand = 5;      // Arranque del loop de CPU/emulación
    std::uint32_t serviceCheckTaskId = 0x0E; // Tarea residente 0xE (servicio host requerido por OS 0x80138328)

    // Tareas residentes del host oficial (call sites 0xD365..0xD42F en el ejecutable):
    std::uint32_t powerCommandId = 0x10;    // RequestResidentTask(0x10, {0}, 0): Power ON
    std::uint32_t standbyCommandId = 0x11;  // RequestResidentTask(0x11, {0}, 0): Power OFF / Standby
    std::uint32_t resetCommandId = 0x12;    // RequestResidentTask(0x12, {0}, 0): Reset de CPU/hardware
    std::uint32_t sdCardCommandId = 0x13;   // RequestResidentTask(0x13, {0}, 0): Inserción/cambio de SD Card

    // Semántica de códigos de retorno de RequestResidentTask / GCSim:
    //  0: Éxito (OK)
    //  1: ID de emulador inválido
    //  2: Tarea no registrada en la tabla de slots de GCalAppIF (eax+0x43c)
    //  3: Motor no inicializado / descriptor nulo
    //  4: Comando rechazado por el estado actual de la simulación (ej. Power-ON cuando
    //     ya está encendido, o Standby cuando ya está inactivo). El host oficial ignora
    //     el retorno de forma deliberada (xor ecx, ebp; ret).
    //  5: Error general / puntero de tabla nulo
    // 11 (0xB): Timeout (2000 ms) en TerminateEmulator al solicitar tarea 4 al guest;
    //           evitado al usar TerminateEmulatorEx(handle, 0) que señaliza comando 6
    //           directo al slot 3 y espera a que los hilos de GCSim terminen limpiamente.

    std::uintptr_t lcdImageAccessorRva = 0xD8EF0;
    std::size_t lcdPixelOffset = 0x87CC;
    std::size_t lcdContentWidthOffset = 0x87C4;
    std::size_t lcdContentHeightOffset = 0x87C8;
    std::uint32_t lcdSurfaceWidth = 1024;
    std::uint32_t lcdSurfaceHeight = 512;
    std::uint32_t lcdBytesPerPixel = 4;
    std::uint32_t lcdExpectedContentWidth = 396;
    std::uint32_t lcdExpectedContentHeight = 224;
    std::uintptr_t lcdModeRva = 0xD8BD4;
    std::uintptr_t lcdWidthRva = 0x121D4;
    std::uintptr_t lcdHeightRva = 0x121D8;

    std::uintptr_t cpuExecFlagRva = 0x675430;
    std::uintptr_t gcsimPauseRva = 0x50A4;
    std::uintptr_t gcsimRunRva = 0x50A8;
    std::uintptr_t keySignalObjectRva = 0xBB14;
    std::uintptr_t keyUpdateCallbackRva = 0xEFF8;
    std::uintptr_t keyFlushCallbackRva = 0xEFF0;

    // Optional size gates. Zero means "unknown; rely on SizeOfImage".
    std::uint64_t lcdDllMinBytes = 0;
    std::uint64_t cpuDllMinBytes = 0;
};

inline constexpr Cg50GcalProfile kCg50GcalProfile{};

inline constexpr std::size_t Cg50SurfaceBytes(const Cg50GcalProfile& p)
{
    return static_cast<std::size_t>(p.lcdSurfaceWidth) *
           static_cast<std::size_t>(p.lcdSurfaceHeight) *
           static_cast<std::size_t>(p.lcdBytesPerPixel);
}
