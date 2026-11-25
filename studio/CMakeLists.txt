cmake_minimum_required(VERSION 3.20)
project(CuadernU LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Unified pure C++ engine harvested from sibling CuadernU/nativo/tinta/nucleo
add_library(tinta_nucleo STATIC
    tinta_core/tinta.cpp
    tinta_core/elementos.cpp
    tinta_core/documento.cpp
    tinta_core/edicion.cpp
    tinta_core/formas.cpp
    tinta_core/archivo.cpp
    tinta_core/grabacion.cpp
    tinta_core/reproduccion.cpp
    tinta_core/lienzo_avanzado.cpp
    tinta_core/ingenieria.cpp
    tinta_core/circuitos.cpp
    tinta_core/matematicas_avanzadas.cpp
    tinta_core/importar.cpp
    tinta_core/ajustes.cpp
    tinta_core/api.cpp
    tinta_core/almacen.cpp
    tinta_core/buscador.cpp
    tinta_core/buscador_api.cpp
    tinta_core/clip.cpp
    tinta_core/cronologia.cpp
    tinta_core/duplicacion.cpp
    tinta_core/estado_sesion.cpp
    tinta_core/exportador.cpp
    tinta_core/foco.cpp
    tinta_core/ia_local.cpp
    tinta_core/matematicas.cpp
    tinta_core/pdf.cpp
    tinta_core/perspectiva.cpp
    tinta_core/simulador.cpp
    tinta_core/transaccion_historial.cpp
    tinta_core/transaccion_historial_api.cpp
    tinta_core/unidades.cpp
)
target_compile_features(tinta_nucleo PRIVATE cxx_std_20)
target_include_directories(tinta_nucleo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tinta_core")
if(MSVC)
    target_compile_options(tinta_nucleo PRIVATE /utf-8 /EHsc /W2)
endif()

set(DOC_CORE_SOURCES
    core/spatial_index.cpp
    core/page_layout.cpp
    core/zip_container.cpp
    core/document_model.cpp
    core/command_stack.cpp
    core/cudoc_storage.cpp
    core/ink_engine.cpp
    core/knowledge_graph.cpp
    core/notebook_vault.cpp
    core/structured_database.cpp
    core/smart_script_reflow.cpp
    core/collaboration_publish_sync.cpp
    core/audio_timeline.cpp
    core/studio_profile_store.cpp
    core/ai_provider_vault.cpp
    core/accessibility_engine.cpp
    core/study_assistant_tools.cpp
    math/engineering_cas.cpp
    math/reactive_math_chemistry.cpp
    circuit/mna_solver.cpp
    bridge/calc_bridge.cpp
    pdf/pdf_parser.cpp
    pdf/pdf_writer.cpp
    pdf/winrt_adapters.cpp
    pdf/pdf_signer.cpp
    pdf/interop_openxml.cpp
    pdf/table_interop.cpp
    pdf/flexcil_study_overlay.cpp
    interop/notebook_migration_engine.cpp
    interop/universal_formats_clipper.cpp
    interop/notion_markdown.cpp
    interop/printout_engine.cpp
    media/native_video_engine.cpp
    media/webview2_html_host.cpp
    render/d2d_renderer.cpp
)

option(CUADERNU_ENABLE_GEMINI_NOTEBOOK "Enable optional decoupled Google Gemini Notebook REST/MCP integration" ON)
if(CUADERNU_ENABLE_GEMINI_NOTEBOOK)
    list(APPEND DOC_CORE_SOURCES
        integrations/gemini_notebook/gemini_notebook_sync_store.cpp
        integrations/gemini_notebook/gemini_notebook_client.cpp
    )
endif()

add_library(DocumentCore STATIC ${DOC_CORE_SOURCES})
target_compile_definitions(DocumentCore PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(CUADERNU_ENABLE_GEMINI_NOTEBOOK)
    target_compile_definitions(DocumentCore PUBLIC DOC_STUDIO_HAS_GEMINI_NOTEBOOK=1)
endif()
if(MSVC)
    target_compile_options(DocumentCore PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(DocumentCore PUBLIC
    tinta_nucleo
    ZLIB::ZLIB
    nlohmann_json::nlohmann_json
    d2d1
    dwrite
    windowscodecs
    crypt32
    ole32
    oleaut32
    comdlg32
    shell32
    user32
    gdi32
    advapi32
    bcrypt
    winmm
    winhttp
    ws2_32
    runtimeobject
    mfplat
    mfreadwrite
    mfuuid
    mf
    shlwapi
    psapi
)

# Optional static Microsoft Edge WebView2 SDK (lazy-loaded on demand only for HtmlEmbed blocks)
set(_WV2_CANDIDATES
    "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/1.0.3179.45/build/native"
    "$ENV{USERPROFILE}/.nuget/packages/microsoft.web.webview2/1.0.2903.40/build/native"
)
foreach(_wv2_dir IN LISTS _WV2_CANDIDATES)
    if(EXISTS "${_wv2_dir}/include/WebView2.h" AND EXISTS "${_wv2_dir}/x64/WebView2LoaderStatic.lib")
        target_include_directories(DocumentCore PRIVATE "${_wv2_dir}/include")
        target_link_libraries(DocumentCore PUBLIC "${_wv2_dir}/x64/WebView2LoaderStatic.lib")
        target_compile_definitions(DocumentCore PUBLIC DOC_STUDIO_HAS_WEBVIEW2=1)
        message(STATUS "CuadernU: Static Microsoft Edge WebView2 enabled from ${_wv2_dir}")
        break()
    endif()
endforeach()

set(ACADEMIC_ENABLE_HORARIOS_TEC ON CACHE BOOL "" FORCE)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../academic/CMakeLists.txt")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../academic" "${CMAKE_CURRENT_BINARY_DIR}/academic_build")
endif()

add_library(CuadernuTecIntegration STATIC
    integrations/tec_digital/tec_academic_store.cpp
)
target_compile_definitions(CuadernuTecIntegration PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(CuadernuTecIntegration PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(CuadernuTecIntegration PUBLIC
    DocumentCore
    academic_tec_digital
    academic_horarios_tec
)

set(CUADERNU_APP_SOURCES
    app/main_studio.cpp
    app/studio_tec_digital_dialog.cpp
)
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/cuadernu/cuadernu.rc")
    list(APPEND CUADERNU_APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../assets/cuadernu/cuadernu.rc")
endif()

add_executable(CuadernU WIN32 ${CUADERNU_APP_SOURCES})
target_compile_definitions(CuadernU PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN DOC_STUDIO_HAS_ACADEMIC=1)
if(MSVC)
    target_compile_options(CuadernU PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(CuadernU PRIVATE
    DocumentCore
    CuadernuTecIntegration
    academic_tec_digital
    academic_horarios_tec
    comctl32
    gdiplus
)

# Produce DocumentStudio.exe compatibility alias alongside official CuadernU.exe
add_custom_command(TARGET CuadernU POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:CuadernU>"
        "$<TARGET_FILE_DIR:CuadernU>/DocumentStudio.exe"
)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)
foreach(_crt_dll IN LISTS CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS)
    if(_crt_dll MATCHES ".*(msvcp140|vcruntime140).*\\.dll$")
        add_custom_command(TARGET CuadernU POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_crt_dll}"
                "$<TARGET_FILE_DIR:CuadernU>"
        )
    endif()
endforeach()

enable_testing()
add_executable(DocumentCoreTest tests/document_core_test.cpp)
target_compile_definitions(DocumentCoreTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(DocumentCoreTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(DocumentCoreTest PRIVATE DocumentCore)
add_dependencies(DocumentCoreTest CuadernU)

set(_studio_manuals_dir "${CMAKE_CURRENT_SOURCE_DIR}/../private_local/manuales")
if(NOT EXISTS "${_studio_manuals_dir}")
    set(_studio_manuals_dir "${CMAKE_CURRENT_SOURCE_DIR}/../manuales")
endif()
add_test(
    NAME DocumentCoreTest
    COMMAND DocumentCoreTest "${_studio_manuals_dir}"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tinta_core/tests/pruebas.cpp")
    add_executable(TintaNucleoTest tinta_core/tests/pruebas.cpp)
    target_link_libraries(TintaNucleoTest PRIVATE tinta_nucleo)
    if(MSVC)
        target_compile_options(TintaNucleoTest PRIVATE /utf-8 /EHsc /W2)
    endif()
    add_test(NAME TintaNucleoTest COMMAND TintaNucleoTest)
endif()

add_executable(CuadernuInteropTest tests/cuadernu_interop_test.cpp)
target_compile_definitions(CuadernuInteropTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(CuadernuInteropTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(CuadernuInteropTest PRIVATE DocumentCore tinta_nucleo)
add_test(NAME CuadernuInteropTest COMMAND CuadernuInteropTest)

add_executable(PrintoutTest tests/printout_test.cpp)
target_compile_definitions(PrintoutTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(PrintoutTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(PrintoutTest PRIVATE DocumentCore tinta_nucleo)
add_test(NAME PrintoutTest COMMAND PrintoutTest "${CMAKE_CURRENT_BINARY_DIR}/printout_fixtures")

add_executable(KnowledgeGraphAndBasesTest tests/knowledge_graph_and_bases_test.cpp)
target_compile_definitions(KnowledgeGraphAndBasesTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(KnowledgeGraphAndBasesTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(KnowledgeGraphAndBasesTest PRIVATE DocumentCore tinta_nucleo)
add_test(NAME KnowledgeGraphAndBasesTest COMMAND KnowledgeGraphAndBasesTest "${CMAKE_CURRENT_BINARY_DIR}/m1_m2_report.json")

add_executable(FlexcilSmartScriptAndMathTest tests/flexcil_smart_script_and_math_test.cpp)
target_compile_definitions(FlexcilSmartScriptAndMathTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(FlexcilSmartScriptAndMathTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(FlexcilSmartScriptAndMathTest PRIVATE DocumentCore tinta_nucleo)
add_test(NAME FlexcilSmartScriptAndMathTest COMMAND FlexcilSmartScriptAndMathTest "${CMAKE_CURRENT_BINARY_DIR}/m3_m4_m5_report.json")

add_executable(AudioCollabSyncAndClipperTest tests/audio_collab_sync_and_clipper_test.cpp)
target_compile_definitions(AudioCollabSyncAndClipperTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(AudioCollabSyncAndClipperTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(AudioCollabSyncAndClipperTest PRIVATE DocumentCore tinta_nucleo)
add_test(NAME AudioCollabSyncAndClipperTest COMMAND AudioCollabSyncAndClipperTest "${CMAKE_CURRENT_BINARY_DIR}/m6_m7_m8_report.json")

add_executable(Full987MatrixComplianceTest tests/full_987_matrix_compliance_test.cpp)
target_compile_definitions(Full987MatrixComplianceTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(Full987MatrixComplianceTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(Full987MatrixComplianceTest PRIVATE DocumentCore tinta_nucleo)
set(CUADERNU_MATRIX_CSV "" CACHE FILEPATH "Canonical CuadernU tracking CSV for the metadata test")
if(CUADERNU_MATRIX_CSV)
    if(NOT EXISTS "${CUADERNU_MATRIX_CSV}" OR IS_DIRECTORY "${CUADERNU_MATRIX_CSV}")
        message(FATAL_ERROR "CUADERNU_MATRIX_CSV must point to the canonical CSV")
    endif()
    add_test(NAME Full987MatrixComplianceTest
        COMMAND Full987MatrixComplianceTest "${CUADERNU_MATRIX_CSV}")
    set_tests_properties(Full987MatrixComplianceTest PROPERTIES LABELS "tracking")
endif()

add_executable(RealE2EClosureTest tests/real_e2e_closure_test.cpp)
target_compile_definitions(RealE2EClosureTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(RealE2EClosureTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(RealE2EClosureTest PRIVATE DocumentCore tinta_nucleo)
add_test(
    NAME RealE2EClosureTest
    COMMAND RealE2EClosureTest "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_executable(AiVaultA11yStudyTest tests/ai_vault_a11y_study_test.cpp)
target_compile_definitions(AiVaultA11yStudyTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(AiVaultA11yStudyTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(AiVaultA11yStudyTest PRIVATE DocumentCore tinta_nucleo)
add_test(NAME AiVaultA11yStudyTest COMMAND AiVaultA11yStudyTest)

add_executable(PerfAndRichMediaTest tests/perf_and_rich_media_test.cpp)
target_compile_definitions(PerfAndRichMediaTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(PerfAndRichMediaTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(PerfAndRichMediaTest PRIVATE DocumentCore tinta_nucleo gdiplus)
add_test(
    NAME PerfAndRichMediaTest
    COMMAND PerfAndRichMediaTest "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(CUADERNU_ENABLE_GEMINI_NOTEBOOK)
    add_executable(GeminiNotebookAndInlineUrlTest tests/gemini_notebook_and_inline_url_test.cpp)
    target_compile_definitions(GeminiNotebookAndInlineUrlTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(GeminiNotebookAndInlineUrlTest PRIVATE /W4 /utf-8 /EHsc)
    endif()
    target_link_libraries(GeminiNotebookAndInlineUrlTest PRIVATE DocumentCore tinta_nucleo gdiplus)
    add_test(
        NAME GeminiNotebookAndInlineUrlTest
        COMMAND GeminiNotebookAndInlineUrlTest "${CMAKE_CURRENT_SOURCE_DIR}" "$<TARGET_FILE:CuadernU>"
    )
endif()

add_executable(TecDigitalIntegrationTest tests/tec_digital_integration_test.cpp)
target_compile_definitions(TecDigitalIntegrationTest PRIVATE _UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(TecDigitalIntegrationTest PRIVATE /W4 /utf-8 /EHsc)
endif()
target_link_libraries(TecDigitalIntegrationTest PRIVATE CuadernuTecIntegration tinta_nucleo gdiplus)
add_test(
    NAME TecDigitalIntegrationTest
    COMMAND TecDigitalIntegrationTest "${CMAKE_CURRENT_SOURCE_DIR}"
)





