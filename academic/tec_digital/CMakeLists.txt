cmake_minimum_required(VERSION 3.20)
project(academic_tec_digital LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# nlohmann_json: x64-windows vcpkg, Linux /usr/share/cmake, or header-only fallback for Win32
find_package(nlohmann_json CONFIG QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
        HINTS
            "${VCPKG_INSTALLED_DIR}/x64-windows/include"
            "D:/DevTools/vcpkg/installed/x64-windows/include"
            "/usr/include"
        NO_CMAKE_FIND_ROOT_PATH
    )
    if(NOT NLOHMANN_JSON_INCLUDE_DIR)
        message(FATAL_ERROR "nlohmann/json.hpp not found for academic_tec_digital")
    endif()
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}"
    )
endif()

if(WIN32)
    find_package(ZLIB QUIET)
else()
    find_package(ZLIB REQUIRED)
    find_package(OpenSSL REQUIRED)
    find_package(Threads REQUIRED)
endif()

set(TEC_DIGITAL_SOURCES
    src/types.cpp
    src/cookie_jar.cpp
    src/http_common.cpp
    src/vault_common.cpp
    src/canon.cpp
    src/policy.cpp
    src/sanitizer.cpp
    src/auth_client.cpp
    src/inflate.cpp
    src/material_indexer.cpp
    src/storage.cpp
    src/academic_parsers.cpp
    src/tda_tds_client.cpp
    src/graphql_client.cpp
    src/public_guia_client.cpp
    src/service.cpp
)

if(WIN32)
    list(APPEND TEC_DIGITAL_SOURCES
        src/crypto_win.cpp
        src/http_transport_win.cpp
        src/vault_win.cpp
    )
else()
    list(APPEND TEC_DIGITAL_SOURCES
        src/crypto_linux.cpp
        src/http_transport_linux.cpp
        src/vault_linux.cpp
    )
endif()

add_library(academic_tec_digital STATIC ${TEC_DIGITAL_SOURCES})
add_library(Academic::TecDigital ALIAS academic_tec_digital)

target_include_directories(academic_tec_digital
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_features(academic_tec_digital PUBLIC cxx_std_17)

if(MSVC)
    target_compile_definitions(academic_tec_digital PUBLIC _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(academic_tec_digital PRIVATE /W4 /utf-8 /EHsc)
else()
    target_compile_options(academic_tec_digital PRIVATE -Wall -Wextra)
endif()

target_link_libraries(academic_tec_digital PUBLIC
    nlohmann_json::nlohmann_json
)

if(TARGET ZLIB::ZLIB)
    target_link_libraries(academic_tec_digital PUBLIC ZLIB::ZLIB)
    target_compile_definitions(academic_tec_digital PUBLIC ACADEMIC_HAS_ZLIB=1)
endif()

if(WIN32)
    target_link_libraries(academic_tec_digital PUBLIC
        winhttp
        bcrypt
        crypt32
        ws2_32
    )
else()
    target_link_libraries(academic_tec_digital PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
    )
endif()

add_executable(tec_digital_cli
    src/tec_digital_cli.cpp
)
target_link_libraries(tec_digital_cli PRIVATE academic_tec_digital)
if(MSVC)
    target_compile_definitions(tec_digital_cli PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(tec_digital_cli PRIVATE /W4 /utf-8 /EHsc)
else()
    target_compile_options(tec_digital_cli PRIVATE -Wall -Wextra)
endif()

if(BUILD_TESTING)
    add_executable(test_tec_foundation
        tests/test_tec_foundation.cpp
    )
    target_link_libraries(test_tec_foundation PRIVATE academic_tec_digital)
    if(MSVC)
        target_compile_definitions(test_tec_foundation PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_options(test_tec_foundation PRIVATE /W4 /utf-8 /EHsc)
    else()
        target_compile_options(test_tec_foundation PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME TecDigitalFoundationTest COMMAND test_tec_foundation)

    add_executable(test_tec_academic
        tests/test_tec_academic.cpp
    )
    target_link_libraries(test_tec_academic PRIVATE academic_tec_digital)
    if(MSVC)
        target_compile_definitions(test_tec_academic PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_options(test_tec_academic PRIVATE /W4 /utf-8 /EHsc)
    else()
        target_compile_options(test_tec_academic PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME TecDigitalAcademicTest COMMAND test_tec_academic)
endif()
