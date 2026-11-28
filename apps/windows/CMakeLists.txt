cmake_minimum_required(VERSION 3.20)
project(CalculadoraUnificadaWindows LANGUAGES C CXX RC)

include("${CMAKE_CURRENT_SOURCE_DIR}/../../tools/cmake/C23Cpp23Standards.cmake")

get_filename_component(MONOREPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CORE_DIR "${MONOREPO_ROOT}/core")
set(ASSETS_DIR "${MONOREPO_ROOT}/assets")
set(PRIVATE_DIR "${MONOREPO_ROOT}/private_local")

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ============================================================================
# Mode A: Win32 (x86) build -> Isolated 32-bit Official Casio DLL Helper
# ============================================================================
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(STATUS "Win32 x86 build: compiling isolated official Casio DLL host (cg50_official_gcal_host32.exe)")
    add_executable(cg50_official_gcal_host32
        cg50_official_gcal_host32.cpp
        cg50_backend.cpp
    )
    target_compile_definitions(cg50_official_gcal_host32 PRIVATE
        UNICODE _UNICODE
        CASIO_MONOREPO_ROOT=L"${MONOREPO_ROOT}"
    )
    target_include_directories(cg50_official_gcal_host32 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    if(MSVC)
        target_compile_options(cg50_official_gcal_host32 PRIVATE /utf-8 /W4 /EHsc)
    endif()
    target_link_libraries(cg50_official_gcal_host32 PRIVATE user32 psapi)
    return()
endif()

# ============================================================================
# Mode B: x64 build -> Unified 64-bit CalculadoraUnificada.exe
#   - Links single-source core/casio_nx, core/casio_cw2, core/casio_cg50 (v47)
#   - Supports 3 runtime CG50 modes:
#       1) NativeIndependent (SH7305 v47 in-process, zero Casio DLLs)
#       2) OfficialCasioDll  (isolated cg50_official_gcal_host32.exe over IPC)
#       3) CompareLive       (lockstep Official vs. Native side-by-side comparator)
# ============================================================================
set(BUILD_CORE_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CG50_BATTERY OFF CACHE BOOL "" FORCE)
add_subdirectory("${CORE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/core_build")

# Embed ClassWiz (fx-570LA X) & ES PLUS (fx-570VN PLUS) ROMs from monorepo assets/roms/
set(ROM_CW_FILE "${ASSETS_DIR}/roms/fx991lax.rom")
set(ROM_ES_FILE "${ASSETS_DIR}/roms/fx570vnplus.rom")
set(GENERATED_ROMS_RC "${CMAKE_CURRENT_BINARY_DIR}/generated_roms.rc")
file(WRITE "${GENERATED_ROMS_RC}" "/* generated_roms.rc */\n")
if(EXISTS "${ROM_CW_FILE}")
    file(TO_NATIVE_PATH "${ROM_CW_FILE}" _rom_cw_native)
    string(REPLACE "\\" "\\\\" _rom_cw_native "${_rom_cw_native}")
    file(APPEND "${GENERATED_ROMS_RC}" "3001 RCDATA \"${_rom_cw_native}\"\n")
    message(STATUS "ROM ClassWiz: ${ROM_CW_FILE}")
endif()
if(EXISTS "${ROM_ES_FILE}")
    file(TO_NATIVE_PATH "${ROM_ES_FILE}" _rom_es_native)
    string(REPLACE "\\" "\\\\" _rom_es_native "${_rom_es_native}")
    file(APPEND "${GENERATED_ROMS_RC}" "3002 RCDATA \"${_rom_es_native}\"\n")
    message(STATUS "ROM Natural ES: ${ROM_ES_FILE}")
endif()

add_executable(CalculadoraUnificada WIN32
    main.cpp
    fx991cw_host.cpp
    cg50_dual_backend.cpp
    app.rc
    ${GENERATED_ROMS_RC}
)

target_compile_definitions(CalculadoraUnificada PRIVATE
    UNICODE
    _UNICODE
    CASIO_MONOREPO_ROOT=L"${MONOREPO_ROOT}"
    CASIO_REPO_DIR=L"${MONOREPO_ROOT}"
)

target_include_directories(CalculadoraUnificada PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${MONOREPO_ROOT}"
)

if(MSVC)
    target_compile_options(CalculadoraUnificada PRIVATE /utf-8 /W3 /EHsc /bigobj)
endif()

target_link_libraries(CalculadoraUnificada PRIVATE
    casio_nx
    casio_cw2
    casio_cg50
    user32
    gdi32
    shell32
    comctl32
    shlwapi
    msimg32
    psapi
    gdiplus
    comdlg32
)

set_target_properties(CalculadoraUnificada PROPERTIES
    LINK_FLAGS "/MANIFEST:NO"
)

# Stage catalog, optional 32-bit official helper, optional CuadernU x64, and optional runtime next to exe
if(EXISTS "${ASSETS_DIR}/catalog")
    add_custom_command(TARGET CalculadoraUnificada POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:CalculadoraUnificada>/catalog"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${ASSETS_DIR}/catalog"
            "$<TARGET_FILE_DIR:CalculadoraUnificada>/catalog"
        COMMENT "Staging CASIO knowledge catalog"
    )
endif()

set(_gcal32_helper "${MONOREPO_ROOT}/build/windows-gcal32/Release/cg50_official_gcal_host32.exe")
if(EXISTS "${_gcal32_helper}")
    add_custom_command(TARGET CalculadoraUnificada POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_gcal32_helper}"
            "$<TARGET_FILE_DIR:CalculadoraUnificada>/cg50_official_gcal_host32.exe"
        COMMENT "Staging 32-bit official Casio DLL helper alongside CalculadoraUnificada.exe"
    )
endif()

set(_studio_bin_dir "${MONOREPO_ROOT}/build/studio-x64/Release")
if(EXISTS "${_studio_bin_dir}/CuadernU.exe")
    add_custom_command(TARGET CalculadoraUnificada POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_studio_bin_dir}/CuadernU.exe"
            "$<TARGET_FILE_DIR:CalculadoraUnificada>/CuadernU.exe"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_studio_bin_dir}/CuadernU.exe"
            "$<TARGET_FILE_DIR:CalculadoraUnificada>/DocumentStudio.exe"
        COMMENT "Staging CuadernU.exe x64 alongside CalculadoraUnificada.exe"
    )
    if(EXISTS "${_studio_bin_dir}/z.dll")
        add_custom_command(TARGET CalculadoraUnificada POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_studio_bin_dir}/z.dll"
                "$<TARGET_FILE_DIR:CalculadoraUnificada>/z.dll"
        )
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(Cg50DualCompareTest
        tests/cg50_dual_compare_test.cpp
        cg50_dual_backend.cpp
    )
    target_compile_definitions(Cg50DualCompareTest PRIVATE
        UNICODE _UNICODE
        CASIO_MONOREPO_ROOT=L"${MONOREPO_ROOT}"
    )
    target_include_directories(Cg50DualCompareTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${MONOREPO_ROOT}"
    )
    if(MSVC)
        target_compile_options(Cg50DualCompareTest PRIVATE /utf-8 /W4 /EHsc)
    endif()
    target_link_libraries(Cg50DualCompareTest PRIVATE casio_cg50 user32 psapi)
    add_test(NAME Cg50DualCompareTest COMMAND Cg50DualCompareTest)

    add_executable(UiCoreModulesTest
        tests/ui_core_modules_test.cpp
    )
    target_compile_definitions(UiCoreModulesTest PRIVATE
        UNICODE _UNICODE
        CASIO_MONOREPO_ROOT=L"${MONOREPO_ROOT}"
        CASIO_REPO_DIR=L"${MONOREPO_ROOT}"
    )
    target_include_directories(UiCoreModulesTest PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${MONOREPO_ROOT}"
    )
    if(MSVC)
        target_compile_options(UiCoreModulesTest PRIVATE /utf-8 /W3 /EHsc)
    endif()
    target_link_libraries(UiCoreModulesTest PRIVATE casio_nx user32 gdi32 gdiplus shlwapi ole32 shell32 comdlg32)
    add_test(NAME UiCoreModules COMMAND UiCoreModulesTest)
endif()
