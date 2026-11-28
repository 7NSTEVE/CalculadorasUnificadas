cmake_minimum_required(VERSION 3.20)
project(AcademicHorariosTec LANGUAGES C CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/cmake/C23Cpp23Standards.cmake")

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    endif()
endif()

# Locate header-only nlohmann_json across:
# 1) Active toolchain / system CMake config (Win x64 vcpkg, Linux /usr/share/cmake/nlohmann_json)
# 2) Windows Win32 builds where vcpkg only has x64-windows installed (header-only, arch-agnostic)
# 3) Direct header fallback
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json CONFIG QUIET)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json AND WIN32)
    find_package(nlohmann_json CONFIG QUIET
        PATHS "D:/DevTools/vcpkg/installed/x64-windows/share/nlohmann_json"
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_path(ACADEMIC_HORARIOS_NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
        HINTS
            "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
            "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include"
            "${VCPKG_INSTALLED_DIR}/x64-windows/include"
            "D:/DevTools/vcpkg/installed/x64-windows/include"
            "/usr/include"
        NO_CMAKE_FIND_ROOT_PATH
        REQUIRED
    )
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ACADEMIC_HORARIOS_NLOHMANN_JSON_INCLUDE_DIR}"
    )
endif()

add_library(academic_horarios_tec STATIC
    src/parser.cpp
    src/solver.cpp
    src/exporter.cpp
)
add_library(academic::horarios_tec ALIAS academic_horarios_tec)

target_include_directories(academic_horarios_tec PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(academic_horarios_tec PUBLIC cxx_std_23)

# Keep <nlohmann/json.hpp> inside .cpp implementation files so consumers
# linking academic::horarios_tec never inherit directory-scoped imported targets.
target_link_libraries(academic_horarios_tec PRIVATE
    nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_definitions(academic_horarios_tec PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
    target_compile_options(academic_horarios_tec PRIVATE /W4 /utf-8 /EHsc)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(academic_horarios_tec PRIVATE -Wall -Wextra -Wconversion -Wpedantic)
endif()

add_executable(horarios_tec_cli
    src/horarios_tec_cli.cpp
)
target_link_libraries(horarios_tec_cli PRIVATE
    academic_horarios_tec
    nlohmann_json::nlohmann_json
)
if(WIN32)
    target_link_libraries(horarios_tec_cli PRIVATE winhttp)
endif()
target_compile_features(horarios_tec_cli PRIVATE cxx_std_23)
if(MSVC)
    target_compile_definitions(horarios_tec_cli PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(horarios_tec_cli PRIVATE /W4 /utf-8 /EHsc)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(horarios_tec_cli PRIVATE -Wall -Wextra)
endif()

option(ACADEMIC_HORARIOS_BUILD_TESTS "Build horarios_tec unit and stress tests" ON)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    include(CTest)
endif()

set(_academic_horarios_run_tests OFF)
if(ACADEMIC_HORARIOS_BUILD_TESTS)
    if(DEFINED BUILD_TESTING)
        if(BUILD_TESTING)
            set(_academic_horarios_run_tests ON)
        endif()
    elseif(CMAKE_TESTING_ENABLED OR (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR))
        set(_academic_horarios_run_tests ON)
    endif()
endif()

if(_academic_horarios_run_tests)
    enable_testing()
    add_executable(test_horarios_tec
        tests/test_horarios_tec.cpp
    )
    target_link_libraries(test_horarios_tec PRIVATE
        academic_horarios_tec
    )
    target_compile_features(test_horarios_tec PRIVATE cxx_std_23)
    if(MSVC)
        target_compile_definitions(test_horarios_tec PRIVATE
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
        target_compile_options(test_horarios_tec PRIVATE /W4 /utf-8 /EHsc)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_horarios_tec PRIVATE -Wall -Wextra -Wconversion -Wpedantic)
    endif()
    add_test(NAME academic_horarios_tec_tests COMMAND test_horarios_tec)
endif()
