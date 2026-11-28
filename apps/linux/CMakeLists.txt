cmake_minimum_required(VERSION 3.20)
project(CalculadoraUnificadaLinux LANGUAGES C CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/cmake/C23Cpp23Standards.cmake")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
endif()

# Must precede add_subdirectory(engines) so BUILD_TESTING is ON when engines & doc_studio_linux check it.
include(CTest)

set(ACADEMIC_ENABLE_HORARIOS_TEC ON CACHE BOOL "" FORCE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../academic/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../academic" "${CMAKE_CURRENT_BINARY_DIR}/academic_build")
endif()

add_subdirectory(engines)
add_subdirectory(doc_studio_linux)
add_subdirectory(app)
add_subdirectory(doc_studio_app)
