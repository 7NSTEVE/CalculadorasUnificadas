cmake_minimum_required(VERSION 3.16)
project(CasioLinuxEngines LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(MONOREPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(CORE_ROOT "${MONOREPO_ROOT}/core")

if(NOT EXISTS "${CORE_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "Cannot find canonical core/CMakeLists.txt at ${CORE_ROOT}")
endif()

find_package(Threads REQUIRED)

if(NOT TARGET casio_native)
    set(BUILD_CORE_TESTS OFF CACHE BOOL "" FORCE)
    set(BUILD_CG50_BATTERY OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CORE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/core_build")
endif()

add_library(CasioLinuxEngines STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/unified_engines.cpp"
)

target_include_directories(CasioLinuxEngines
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CORE_ROOT}/casio_ffi"
    PRIVATE
        "${CORE_ROOT}/casio_nx/include"
        "${CORE_ROOT}/casio_cw2/include"
        "${CORE_ROOT}/casio_cg50"
)

target_compile_definitions(CasioLinuxEngines
    PRIVATE
        CASIO_SOURCE_REPO_DIR="${MONOREPO_ROOT}"
)

target_compile_features(CasioLinuxEngines PUBLIC cxx_std_23)

target_link_libraries(CasioLinuxEngines
    PUBLIC
        casio_native
        casio_nx
        casio_cw2
        casio_cg50
        Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/unified_engines.cpp"
        PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic"
    )
endif()

option(CASIO_ENGINES_BUILD_TESTS "Build CasioLinuxEngines unit & integration tests" ON)
if(CASIO_ENGINES_BUILD_TESTS AND (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR BUILD_TESTING))
    include(CTest)
    enable_testing()
    add_executable(test_linux_engines
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_linux_engines.cpp"
    )
    target_link_libraries(test_linux_engines PRIVATE CasioLinuxEngines)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(test_linux_engines PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME test_linux_engines COMMAND test_linux_engines)
endif()
