cmake_minimum_required(VERSION 3.20)
project(academic_matricula_tec LANGUAGES C CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/cmake/C23Cpp23Standards.cmake")

set(MATRICULA_TEC_SOURCES
    src/matricula_c23_codec.c
    src/protocol.cpp
    src/scenario_engine.cpp
    src/client.cpp
    src/scheduler.cpp
)

if(WIN32)
    list(APPEND MATRICULA_TEC_SOURCES
        src/vault_win.cpp
        src/scraper_bridge_win.cpp
    )
endif()

add_library(academic_matricula_tec STATIC ${MATRICULA_TEC_SOURCES})
add_library(Academic::MatriculaTec ALIAS academic_matricula_tec)
cu_apply_c23_cpp23(academic_matricula_tec)

target_include_directories(academic_matricula_tec
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

if(MSVC)
    target_compile_definitions(academic_matricula_tec PUBLIC _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(academic_matricula_tec PRIVATE /W4)
else()
    target_compile_options(academic_matricula_tec PRIVATE -Wall -Wextra)
endif()

target_link_libraries(academic_matricula_tec PUBLIC
    academic_tec_digital
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(academic_matricula_tec PUBLIC
        winhttp
        crypt32
        ws2_32
        advapi32
    )
endif()

add_executable(matricula_tec_cli
    src/matricula_tec_cli.cpp
)
cu_apply_c23_cpp23(matricula_tec_cli)
target_link_libraries(matricula_tec_cli PRIVATE academic_matricula_tec)
if(MSVC)
    target_compile_definitions(matricula_tec_cli PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(matricula_tec_cli PRIVATE /W4)
endif()

if(BUILD_TESTING)
    add_executable(test_matricula_tec
        tests/test_matricula_tec.cpp
    )
    cu_apply_c23_cpp23(test_matricula_tec)
    target_link_libraries(test_matricula_tec PRIVATE academic_matricula_tec)
    if(MSVC)
        target_compile_definitions(test_matricula_tec PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_options(test_matricula_tec PRIVATE /W4)
    endif()
    add_test(NAME MatriculaTecTest COMMAND test_matricula_tec)
endif()
