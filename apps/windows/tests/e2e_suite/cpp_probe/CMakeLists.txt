cmake_minimum_required(VERSION 3.20)
project(E2ECppProbe LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(DOC_STUDIO_ROOT "${REPO_ROOT}/doc_studio")

find_package(ZLIB REQUIRED)
find_package(nlohmann_json QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp REQUIRED)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}"
    )
endif()

add_executable(e2e_cpp_probe probe_main.cpp)
target_include_directories(e2e_cpp_probe PRIVATE "${DOC_STUDIO_ROOT}")

if(WIN32)
    set(DOC_CORE_LIB "${REPO_ROOT}/build-studio-x64/Release/DocumentCore.lib"
        CACHE FILEPATH "Path to compiled DocumentCore.lib")
    if(NOT EXISTS "${DOC_CORE_LIB}")
        message(FATAL_ERROR "DocumentCore.lib not found at ${DOC_CORE_LIB}")
    endif()
    target_compile_definitions(e2e_cpp_probe PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(e2e_cpp_probe PRIVATE /W4 /utf-8 /EHsc)
    endif()
    target_link_libraries(e2e_cpp_probe PRIVATE
        "${DOC_CORE_LIB}"
        ZLIB::ZLIB
        nlohmann_json::nlohmann_json
        d2d1
        dwrite
        windowscodecs
        crypt32
        ole32
        comdlg32
        shell32
        user32
        gdi32
        advapi32
        bcrypt
        runtimeobject
    )
else()
    set(DOC_CORE_LINUX_LIB "${REPO_ROOT}/linux/build-release/doc_studio_linux/libDocumentCoreLinux.a"
        CACHE FILEPATH "Path to compiled libDocumentCoreLinux.a")
    if(NOT EXISTS "${DOC_CORE_LINUX_LIB}")
        message(FATAL_ERROR "libDocumentCoreLinux.a not found at ${DOC_CORE_LINUX_LIB}")
    endif()
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(POPPLER_GLIB REQUIRED IMPORTED_TARGET poppler-glib)
    pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
    pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo)
    pkg_check_modules(GDK_PIXBUF REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)

    target_compile_options(e2e_cpp_probe PRIVATE -Wall -Wextra)
    target_link_libraries(e2e_cpp_probe PRIVATE
        "${DOC_CORE_LINUX_LIB}"
        ZLIB::ZLIB
        nlohmann_json::nlohmann_json
        OpenSSL::Crypto
        PkgConfig::POPPLER_GLIB
        PkgConfig::CAIRO
        PkgConfig::PANGOCAIRO
        PkgConfig::GDK_PIXBUF
        Threads::Threads
    )
endif()
