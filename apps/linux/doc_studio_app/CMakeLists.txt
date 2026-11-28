cmake_minimum_required(VERSION 3.20)
project(DocumentStudioGtk LANGUAGES C CXX)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
if(NOT TARGET PkgConfig::GTK3)
    pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
endif()
find_package(Threads REQUIRED)

if(NOT TARGET DocumentCoreLinux)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../doc_studio_linux/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../doc_studio_linux" doc_studio_linux_build)
    else()
        message(FATAL_ERROR "DocumentCoreLinux target not found and ../doc_studio_linux/CMakeLists.txt does not exist.")
    endif()
endif()

add_executable(document-studio
    main_studio_gtk.cpp
    table_interop.hpp
)

target_include_directories(document-studio PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../doc_studio"
)

target_compile_definitions(document-studio PRIVATE DOC_STUDIO_HAS_ACADEMIC=1)

target_compile_options(document-studio PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

target_link_libraries(document-studio PRIVATE
    DocumentCoreLinux
    academic_tec_digital
    academic_horarios_tec
    PkgConfig::GTK3
    Threads::Threads
)
