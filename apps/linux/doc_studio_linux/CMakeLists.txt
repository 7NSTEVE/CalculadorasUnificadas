cmake_minimum_required(VERSION 3.20)
project(DocumentCoreLinux LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(DOC_STUDIO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../studio" ABSOLUTE)

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(POPPLER_GLIB REQUIRED IMPORTED_TARGET poppler-glib)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo)
pkg_check_modules(GDK_PIXBUF REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)

find_package(nlohmann_json QUIET)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp REQUIRED)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}"
    )
endif()

add_library(DocumentCoreLinux STATIC
    "${DOC_STUDIO_ROOT}/core/spatial_index.cpp"
    "${DOC_STUDIO_ROOT}/core/page_layout.cpp"
    "${DOC_STUDIO_ROOT}/core/zip_container.cpp"
    "${DOC_STUDIO_ROOT}/core/document_model.cpp"
    "${DOC_STUDIO_ROOT}/core/command_stack.cpp"
    "${DOC_STUDIO_ROOT}/core/cudoc_storage.cpp"
    "${DOC_STUDIO_ROOT}/core/ink_engine.cpp"
    "${DOC_STUDIO_ROOT}/core/notebook_vault.cpp"
    "${DOC_STUDIO_ROOT}/core/audio_timeline.cpp"
    "${DOC_STUDIO_ROOT}/math/engineering_cas.cpp"
    "${DOC_STUDIO_ROOT}/circuit/mna_solver.cpp"
    "${DOC_STUDIO_ROOT}/bridge/calc_bridge.cpp"
    "${DOC_STUDIO_ROOT}/pdf/pdf_parser.cpp"
    "${DOC_STUDIO_ROOT}/pdf/pdf_writer.cpp"
    "${DOC_STUDIO_ROOT}/pdf/interop_openxml.cpp"
    "${DOC_STUDIO_ROOT}/pdf/table_interop.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/winrt_adapters_linux.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pdf_signer_linux.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/d2d_renderer_linux.cpp"
)
add_library(DocStudio::DocumentCoreLinux ALIAS DocumentCoreLinux)

target_include_directories(DocumentCoreLinux PUBLIC
    "${DOC_STUDIO_ROOT}"
)

target_link_libraries(DocumentCoreLinux PUBLIC
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
    PkgConfig::POPPLER_GLIB
    PkgConfig::CAIRO
    PkgConfig::PANGOCAIRO
    PkgConfig::GDK_PIXBUF
    Threads::Threads
)

target_compile_features(DocumentCoreLinux PUBLIC cxx_std_23)
target_compile_options(DocumentCoreLinux PRIVATE -Wall -Wextra)

option(DOC_STUDIO_LINUX_BUILD_TESTS "Build DocumentCoreLinux test suite" ON)
if(DOC_STUDIO_LINUX_BUILD_TESTS)
    add_executable(document_core_linux_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/document_core_linux_test.cpp"
    )
    target_link_libraries(document_core_linux_test PRIVATE
        DocumentCoreLinux
        PkgConfig::CAIRO
    )
    target_compile_options(document_core_linux_test PRIVATE -Wall -Wextra)
    enable_testing()
    add_test(
        NAME DocumentCoreLinuxTests
        COMMAND document_core_linux_test "${DOC_STUDIO_ROOT}/../private_local/manuales"
    )
endif()
