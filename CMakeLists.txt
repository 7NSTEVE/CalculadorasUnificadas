cmake_minimum_required(VERSION 3.20)
project(CalculadorasUnificadasMonorepo LANGUAGES CXX)

# Single-Source Monorepo Root CMake entry point.
# Subprojects can also be configured directly into build/<preset>/:
#   - core/          -> 4 certified C++17 calculator engines (v47)
#   - apps/windows/  -> Windows x64 Dual-Engine + Live Comparator & Win32 Official DLL Host
#   - studio/        -> CuadernU / Document Studio x64 + academic/tec_digital
#   - apps/linux/    -> Linux GTK + Cairo desktop port

option(CU_BUILD_CORE "Build canonical native C++ calculator cores" ON)
option(CU_BUILD_WINDOWS_APP "Build Windows desktop app (x64)" OFF)
option(CU_BUILD_STUDIO "Build CuadernU / Document Studio (x64)" OFF)

if(CU_BUILD_CORE)
    add_subdirectory(core)
endif()
if(CU_BUILD_WINDOWS_APP AND WIN32)
    add_subdirectory(apps/windows)
endif()
if(CU_BUILD_STUDIO AND WIN32)
    add_subdirectory(studio)
endif()
