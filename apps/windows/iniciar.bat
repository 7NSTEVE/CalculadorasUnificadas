@echo off
:: Calculadoras CASIO Unificada - canonical executable and explicit working directory.
cd /d "%~dp0"
if exist "%~dp0CalculadoraUnificada.exe" (
    start "" /D "%~dp0" "%~dp0CalculadoraUnificada.exe"
    exit /b 0
)
if exist "%~dp0build-verify\Release\CalculadoraUnificada.exe" (
    start "" /D "%~dp0build-verify\Release" "%~dp0build-verify\Release\CalculadoraUnificada.exe"
    exit /b 0
)
if exist "%~dp0build-win32\Release\CalculadoraUnificada.exe" (
    start "" /D "%~dp0build-win32\Release" "%~dp0build-win32\Release\CalculadoraUnificada.exe"
    exit /b 0
)
if exist "%~dp0build\Release\CalculadoraUnificada.exe" (
    start "" /D "%~dp0build\Release" "%~dp0build\Release\CalculadoraUnificada.exe"
    exit /b 0
)
echo No se encontro el ejecutable.
echo Ejecute compilar.bat primero para compilar el proyecto.
pause
exit /b 1