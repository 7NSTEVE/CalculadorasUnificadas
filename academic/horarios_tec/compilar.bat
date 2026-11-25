@echo off
echo Compilando Visualizador de Horarios...
g++ -O3 main.cpp -o HorariosTEC.exe
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Compilacion exitosa. Ejecute HorariosTEC.exe para abrir la aplicacion interactiva.
) else (
    echo Error durante la compilacion.
)
pause
