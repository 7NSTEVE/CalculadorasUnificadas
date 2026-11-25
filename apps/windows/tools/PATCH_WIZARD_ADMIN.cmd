@echo off
setlocal
if "%~1"=="--elevated" goto :run
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -ArgumentList '--elevated','%~1' -Verb RunAs"
exit /b

:run
cd /d "%~dp0"
set DLL=C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series\ActivationFxMb.dll
set OUT=%~dp0..\build-verify\patch_admin_result.txt
set SETNAME=%~2
if "%SETNAME%"=="" set SETNAME=restore
echo ==== ACCION: %SETNAME% ==== > "%OUT%"
if /I "%SETNAME%"=="restore" (
  "D:\DevTools\Python311\python.exe" "%~dp0patch_activationfxmb.py" "%DLL%" --restore >> "%OUT%" 2>&1
) else (
  "D:\DevTools\Python311\python.exe" "%~dp0patch_activationfxmb.py" "%DLL%" --restore >> "%OUT%" 2>&1
  "D:\DevTools\Python311\python.exe" "%~dp0patch_activationfxmb.py" "%DLL%" --set %SETNAME% >> "%OUT%" 2>&1
)
echo ==== RESULTADO ==== >> "%OUT%"
type "%OUT%"
echo.
pause
