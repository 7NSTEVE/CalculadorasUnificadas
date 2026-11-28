cmake_minimum_required(VERSION 3.20)
project(CalculadoraUnificadaAcademic LANGUAGES C CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/../tools/cmake/C23Cpp23Standards.cmake")

# NOTE: Do NOT force CMAKE_MSVC_RUNTIME_LIBRARY here so standalone x64 (/MD)
# and Win32 (/MT when included or matched) both build cleanly.

include(CTest)

add_subdirectory(tec_digital)

# M3.2 owns academic/horarios_tec/ independently; M3.4 integrates it unconditionally.
option(ACADEMIC_ENABLE_HORARIOS_TEC "Include horarios_tec subdirectory if present" ON)
if(ACADEMIC_ENABLE_HORARIOS_TEC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/horarios_tec/CMakeLists.txt")
    add_subdirectory(horarios_tec)
endif()

option(ACADEMIC_ENABLE_MATRICULA_TEC "Include matricula_tec subdirectory if present" ON)
if(ACADEMIC_ENABLE_MATRICULA_TEC AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/matricula_tec/CMakeLists.txt")
    add_subdirectory(matricula_tec)
endif()

option(ACADEMIC_ENABLE_TEC_MAIL_TRIAGE "Include tec_mail_triage subdirectory if present" ON)
if(ACADEMIC_ENABLE_TEC_MAIL_TRIAGE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tec_mail_triage/CMakeLists.txt")
    add_subdirectory(tec_mail_triage)
endif()
