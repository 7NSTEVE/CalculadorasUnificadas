cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(CalculadoraUnificadaApp LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
if(NOT TARGET PkgConfig::GTK3)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
endif()
if(NOT TARGET PkgConfig::POPPLER_GLIB)
    pkg_check_modules(POPPLER_GLIB REQUIRED IMPORTED_TARGET poppler-glib)
endif()
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_package(nlohmann_json 3.2.0 QUIET)
endif()
find_package(Threads REQUIRED)

if(NOT TARGET CasioLinuxEngines)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../engines/CMakeLists.txt")
        set(CASIO_ENGINES_BUILD_TESTS OFF CACHE BOOL "Disable engine tests during app standalone build")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../engines" engines_build)
    else()
        message(FATAL_ERROR "CasioLinuxEngines target not found and ../engines/CMakeLists.txt does not exist.")
    endif()
endif()

add_executable(calculadora-unificada
    main_gtk.cpp
    latex_cairo.cpp
    knowledge_catalog_linux.cpp
    manual_viewer_gtk.cpp
    exporter_gtk.cpp
)

target_compile_features(calculadora-unificada PRIVATE cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(calculadora-unificada PRIVATE
        -Wall
        -Wextra
        -Werror
    )
endif()

target_include_directories(calculadora-unificada PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_link_libraries(calculadora-unificada PRIVATE
    CasioLinuxEngines
    PkgConfig::GTK3
    PkgConfig::POPPLER_GLIB
    Threads::Threads
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(calculadora-unificada PRIVATE nlohmann_json::nlohmann_json)
endif()
