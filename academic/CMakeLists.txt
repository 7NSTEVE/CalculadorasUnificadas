cmake_minimum_required(VERSION 3.20)
project(CalculadoraUnificadaAcademic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# NOTE: Do NOT force CMAKE_MSVC_RUNTIME_LIBRARY here so standalone x64 (/MD)
# and Win32 (/MT when included or matched) both build cleanly.

include(CTest)

add_subdirectory(tec_digital)

# M3.2 owns academic/horarios_tec/ independently; M3.4 integrates it unconditionally.
option(ACADEMIC_ENABLE_HORARIOS_TEC "Include horarios_tec subdirectory if present" ON)
if(ACADEMIC_ENABLE_HORARIOS_TEC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/horarios_tec/CMakeLists.txt")
    add_subdirectory(horarios_tec)
endif()
