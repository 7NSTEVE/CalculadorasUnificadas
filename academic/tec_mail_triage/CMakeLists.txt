cmake_minimum_required(VERSION 3.20)
project(academic_tec_mail_triage LANGUAGES C CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/cmake/C23Cpp23Standards.cmake")

set(TEC_MAIL_TRIAGE_SOURCES
    src/es_date_course_c23.c
    src/triage_engine.cpp
    src/notebooklm_bundler.cpp
)

if(WIN32)
    list(APPEND TEC_MAIL_TRIAGE_SOURCES
        src/outlook_com_reader_win.cpp
    )
endif()

add_library(academic_tec_mail_triage STATIC ${TEC_MAIL_TRIAGE_SOURCES})
add_library(Academic::TecMailTriage ALIAS academic_tec_mail_triage)
cu_apply_c23_cpp23(academic_tec_mail_triage)

target_include_directories(academic_tec_mail_triage
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(academic_tec_mail_triage PUBLIC
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_compile_definitions(academic_tec_mail_triage PUBLIC _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(academic_tec_mail_triage PUBLIC ole32 oleaut32)
    if(MSVC)
        target_compile_options(academic_tec_mail_triage PRIVATE /W4)
    endif()
else()
    target_compile_options(academic_tec_mail_triage PRIVATE -Wall -Wextra)
endif()

add_executable(tec_mail_triage_cli
    src/tec_mail_triage_cli.cpp
)
cu_apply_c23_cpp23(tec_mail_triage_cli)
target_link_libraries(tec_mail_triage_cli PRIVATE academic_tec_mail_triage)
if(MSVC)
    target_compile_options(tec_mail_triage_cli PRIVATE /W4)
endif()

if(BUILD_TESTING)
    add_executable(test_tec_mail_triage
        tests/test_tec_mail_triage.cpp
    )
    cu_apply_c23_cpp23(test_tec_mail_triage)
    target_link_libraries(test_tec_mail_triage PRIVATE academic_tec_mail_triage)
    if(MSVC)
        target_compile_options(test_tec_mail_triage PRIVATE /W4)
    endif()
    add_test(NAME TecMailTriageTest COMMAND test_tec_mail_triage)
endif()
