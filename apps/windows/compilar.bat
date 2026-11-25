@echo off
:: Build script for Calculadora Unificada (C++ Win32)
:: GCalAppIF is 32-bit (_GCAI_CreateEmulator@28). Default generator platform: Win32.

title Compilando Calculadora Unificada...
cd /d "%~dp0"

set CMAKE="D:\DevTools\CMake\bin\cmake.exe"
if not exist %CMAKE% set CMAKE=cmake

echo [1/3] Configurando proyecto con CMake (Win32)...
if not exist build-verify mkdir build-verify
%CMAKE% -S . -B build-verify -G "Visual Studio 17 2022" -A Win32
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Fallo la configuracion de CMake.
    echo Si faltan ROM, copia fx991lax.rom y fx570vnplus.rom a roms\ o define ROMS_DIR.
    pause
    exit /b 1
)

echo [2/3] Compilando...
%CMAKE% --build build-verify --config Release
if %errorlevel% neq 0 (
    echo.
    echo ERROR: Fallo la compilacion.
    pause
    exit /b 1
)

echo [3/3] Listo!
copy /Y "build-verify\Release\CalculadoraUnificada.exe" "%~dp0CalculadoraUnificada.exe" >nul
echo.
echo Ejecutable produccion: CalculadoraUnificada.exe (y build-verify\Release\CalculadoraUnificada.exe)
echo (build\, build-win32\ y build32\ antiguos se conservan como evidencia; no usar)
echo Las DLL de GCal se buscan junto al exe y en runtime\
echo.
pause
