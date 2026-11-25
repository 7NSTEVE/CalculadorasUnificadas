cmake_minimum_required(VERSION 3.18.1)
project(casio_unified_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_CORE_TESTS "Build native C++ unit/smoke tests for all 4 calculators" OFF)
option(BUILD_CG50_BATTERY "Build CG50 verification battery harness and apps probe" OFF)
option(CG50_ENABLE_COVERAGE_COUNTERS "Enable CG50 opcode/MMIO/state hitmap instrumentation (OFF in production)" OFF)

# 1. core/casio_nx (fx-570LA X ClassWiz + fx-570VN PLUS, namespace casioemu)
add_library(casio_nx STATIC
    casio_nx/src/calcemu.cpp
    casio_nx/src/Data/ModelInfo.cpp
    casio_nx/src/Emulator.cpp
    casio_nx/src/Logger.cpp
    casio_nx/src/Chipset/Chipset.cpp
    casio_nx/src/Chipset/CPU.cpp
    casio_nx/src/Chipset/CPUArithmetic.cpp
    casio_nx/src/Chipset/CPUControl.cpp
    casio_nx/src/Chipset/CPULoadStore.cpp
    casio_nx/src/Chipset/CPUPushPop.cpp
    casio_nx/src/Chipset/InterruptSource.cpp
    casio_nx/src/Chipset/MMU.cpp
    casio_nx/src/Chipset/MMURegion.cpp
    casio_nx/src/Chipset/Snapshot.cpp
    casio_nx/src/Peripheral/BatteryBackedRAM.cpp
    casio_nx/src/Peripheral/Keyboard.cpp
    casio_nx/src/Peripheral/Miscellaneous.cpp
    casio_nx/src/Peripheral/Peripheral.cpp
    casio_nx/src/Peripheral/ROMWindow.cpp
    casio_nx/src/Peripheral/Screen.cpp
    casio_nx/src/Peripheral/StandbyControl.cpp
    casio_nx/src/Peripheral/Timer.cpp
)
target_include_directories(casio_nx
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/casio_nx/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/casio_nx/compat"
            "${CMAKE_CURRENT_SOURCE_DIR}/casio_nx/src"
)
set_target_properties(casio_nx PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 2. core/casio_cw2 (fx-991CW ClassWiz II, 4-gray, namespace casioemux, GPL-3.0)
add_library(casio_cw2 STATIC
    casio_cw2/src/casio_cw2.cpp
    casio_cw2/src/Data/ModelInfo.cpp
    casio_cw2/src/Emulator.cpp
    casio_cw2/src/Logger.cpp
    casio_cw2/src/Chipset/Chipset.cpp
    casio_cw2/src/Chipset/Coprocessor.cpp
    casio_cw2/src/Chipset/CPU.cpp
    casio_cw2/src/Chipset/CPUArithmetic.cpp
    casio_cw2/src/Chipset/CPUControl.cpp
    casio_cw2/src/Chipset/CPULoadStore.cpp
    casio_cw2/src/Chipset/CPUPushPop.cpp
    casio_cw2/src/Chipset/InterruptSource.cpp
    casio_cw2/src/Chipset/MMU.cpp
    casio_cw2/src/Chipset/MMURegion.cpp
    casio_cw2/src/Peripheral/BatteryBackedRAM.cpp
    casio_cw2/src/Peripheral/BCDCalc.cpp
    casio_cw2/src/Peripheral/ExternalInterrupts.cpp
    casio_cw2/src/Peripheral/Flash.cpp
    casio_cw2/src/Peripheral/IOPorts.cpp
    casio_cw2/src/Peripheral/Keyboard.cpp
    casio_cw2/src/Peripheral/Miscellaneous.cpp
    casio_cw2/src/Peripheral/Peripheral.cpp
    casio_cw2/src/Peripheral/PowerSupply.cpp
    casio_cw2/src/Peripheral/RealTimeClock.cpp
    casio_cw2/src/Peripheral/ROMWindow.cpp
    casio_cw2/src/Peripheral/Screen.cpp
    casio_cw2/src/Peripheral/StandbyControl.cpp
    casio_cw2/src/Peripheral/Timer.cpp
    casio_cw2/src/Peripheral/TimerBaseCounter.cpp
    casio_cw2/src/Peripheral/WatchdogTimer.cpp
)
target_include_directories(casio_cw2
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/casio_cw2/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/casio_cw2/compat"
            "${CMAKE_CURRENT_SOURCE_DIR}/casio_cw2/src"
)
set_target_properties(casio_cw2 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 3. core/casio_cg50 (fx-CG50 PRIZM SH7305 + R61524)
add_library(casio_cg50 STATIC
    casio_cg50/cg50_core/r61524.cpp
    casio_cg50/cg50_core/cg50_keyboard_matrix.cpp
    casio_cg50/cg50_core/cg50_peripheral_bus.cpp
    casio_cg50/cg50_core/cg50_usb_host.cpp
    casio_cg50/cg50_core/cg50_lcd_qr.cpp
    casio_cg50/cg50_core/cg50_fugue_ftl.cpp
    casio_cg50/cg50_core/cg50_smem_fs.cpp
    casio_cg50/cg50_core/sh_exception.cpp
    casio_cg50/cg50_core/sh_interrupt.cpp
    casio_cg50/cg50_native_core.cpp
    casio_cg50/cg50_native_runtime.cpp
    casio_cg50/cg50_trace.cpp
)
target_include_directories(casio_cg50
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/casio_cg50"
)
if(CG50_ENABLE_COVERAGE_COUNTERS)
    target_compile_definitions(casio_cg50 PUBLIC CG50_ENABLE_COVERAGE_COUNTERS=1)
endif()
if(APPLE)
    # Prevent Clang de Xcode LLVM O2/Os compile-time blowup (>15m) on 8.6k-line SH7305 decoder TU.
    set_source_files_properties(casio_cg50/cg50_native_core.cpp PROPERTIES COMPILE_OPTIONS "-O1")
endif()
set_target_properties(casio_cg50 PROPERTIES POSITION_INDEPENDENT_CODE ON)

# 4. Portable CuadernU Studio C++17 Engine (shared across Android/iOS/Windows/Linux via casio_native)
option(CUADERNU_BUILD_STUDIO_FFI "Bundle portable CuadernU Studio C++17 engine into casio_native FFI" ON)
set(STUDIO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../studio")
set(ACADEMIC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../academic")
set(_CUADERNU_STUDIO_ENABLED OFF)

if(CUADERNU_BUILD_STUDIO_FFI AND EXISTS "${STUDIO_ROOT}/c_api/cuadernu_studio_ffi.cpp")
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND OR ANDROID OR APPLE)
        set(_CUADERNU_STUDIO_ENABLED ON)
        message(STATUS "CuadernU Studio FFI enabled inside casio_native (portable C++17 + tinta_core + CAS + MNA + .cudoc + HorariosTEC)")
        add_library(cuadernu_studio_portable STATIC
            "${STUDIO_ROOT}/core/spatial_index.cpp"
            "${STUDIO_ROOT}/core/page_layout.cpp"
            "${STUDIO_ROOT}/core/zip_container.cpp"
            "${STUDIO_ROOT}/core/document_model.cpp"
            "${STUDIO_ROOT}/core/command_stack.cpp"
            "${STUDIO_ROOT}/core/cudoc_storage.cpp"
            "${STUDIO_ROOT}/core/ink_engine.cpp"
            "${STUDIO_ROOT}/core/knowledge_graph.cpp"
            "${STUDIO_ROOT}/core/notebook_vault.cpp"
            "${STUDIO_ROOT}/core/structured_database.cpp"
            "${STUDIO_ROOT}/math/engineering_cas.cpp"
            "${STUDIO_ROOT}/math/reactive_math_chemistry.cpp"
            "${STUDIO_ROOT}/circuit/mna_solver.cpp"
            "${STUDIO_ROOT}/pdf/pdf_parser.cpp"
            "${STUDIO_ROOT}/pdf/pdf_writer.cpp"
            "${STUDIO_ROOT}/pdf/interop_openxml.cpp"
            "${STUDIO_ROOT}/pdf/table_interop.cpp"
            "${ACADEMIC_ROOT}/horarios_tec/src/parser.cpp"
            "${ACADEMIC_ROOT}/horarios_tec/src/solver.cpp"
            "${ACADEMIC_ROOT}/horarios_tec/src/exporter.cpp"
            "${STUDIO_ROOT}/c_api/cuadernu_studio_ffi.cpp"
        )
        target_include_directories(cuadernu_studio_portable
            PUBLIC  "${STUDIO_ROOT}/c_api"
            PRIVATE "${STUDIO_ROOT}"
                    "${STUDIO_ROOT}/third_party"
                    "${ACADEMIC_ROOT}/horarios_tec/include"
        )
        if(ZLIB_FOUND)
            target_link_libraries(cuadernu_studio_portable PRIVATE ZLIB::ZLIB)
        else()
            target_link_libraries(cuadernu_studio_portable PRIVATE z)
        endif()
        if(WIN32)
            target_compile_definitions(cuadernu_studio_portable PRIVATE
                _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE CASIO_NATIVE_SHARED=1)
            target_link_libraries(cuadernu_studio_portable PRIVATE windowscodecs ole32 dwrite bcrypt)
        else()
            target_compile_definitions(cuadernu_studio_portable PRIVATE
                DOCSTUDIO_PORTABLE_NO_OPENSSL=1 DOCSTUDIO_PORTABLE_NO_CAIRO=1)
        endif()
        if(MSVC)
            target_compile_options(cuadernu_studio_portable PRIVATE /O2 /EHsc /utf-8)
        elseif(APPLE)
            target_compile_options(cuadernu_studio_portable PRIVATE -Os -fvisibility=hidden)
        else()
            target_compile_options(cuadernu_studio_portable PRIVATE -O3 -fvisibility=hidden)
        endif()
        set_target_properties(cuadernu_studio_portable PROPERTIES POSITION_INDEPENDENT_CODE ON)
    else()
        message(STATUS "CuadernU Studio FFI skipped: ZLIB not found (pass vcpkg toolchain on Windows to enable)")
    endif()
endif()

# 5. Shared FFI library (libcasio_native.so / casio_native.dll)
add_library(casio_native SHARED
    casio_ffi/casio_ffi.cpp
)
target_include_directories(casio_native
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/casio_ffi"
)
target_link_libraries(casio_native PRIVATE casio_nx casio_cw2 casio_cg50)
if(_CUADERNU_STUDIO_ENABLED)
    target_include_directories(casio_native PUBLIC "${STUDIO_ROOT}/c_api")
    if(MSVC)
        target_link_libraries(casio_native PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,cuadernu_studio_portable>)
    elseif(APPLE)
        target_link_libraries(casio_native PRIVATE cuadernu_studio_portable "-Wl,-force_load,$<TARGET_FILE:cuadernu_studio_portable>")
    else()
        target_link_libraries(casio_native PRIVATE -Wl,--whole-archive cuadernu_studio_portable -Wl,--no-whole-archive)
    endif()
endif()

foreach(tgt casio_nx casio_cw2 casio_cg50 casio_native)
    if(WIN32)
        target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /O2 /EHsc /utf-8)
    elseif(CG50_ENABLE_COVERAGE_COUNTERS)
        target_compile_options(${tgt} PRIVATE -O1 -fvisibility=hidden)
    elseif(APPLE)
        target_compile_options(${tgt} PRIVATE -Os -fvisibility=hidden)
    else()
        target_compile_options(${tgt} PRIVATE -O3 -fvisibility=hidden)
    endif()
endforeach()

if(WIN32)
    target_compile_definitions(casio_native PRIVATE CASIO_NATIVE_SHARED=1)
endif()

if(ANDROID)
    # Android 15+ 16 KB ELF page alignment requirement
    target_link_options(casio_native PRIVATE -Wl,-z,max-page-size=16384 -Wl,-z,common-page-size=16384)
endif()

if(BUILD_CORE_TESTS)
    enable_testing()

    set(CORE_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core")
    set(CORE_ROMS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/roms")

    foreach(test_name cg50_android_smoke cw2_headless_test nx_regression_test)
        add_executable(${test_name} "${CORE_TEST_DIR}/${test_name}.cpp")
        target_link_libraries(${test_name} PRIVATE casio_native)
        target_compile_definitions(${test_name} PRIVATE
            CASIO_ROMS_DIR="${CORE_ROMS_DIR}"
            _CRT_SECURE_NO_WARNINGS
        )
        if(MSVC)
            target_compile_options(${test_name} PRIVATE /O2 /EHsc /utf-8)
        endif()
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()

    if(_CUADERNU_STUDIO_ENABLED)
        add_executable(cuadernu_studio_ffi_test "${CORE_TEST_DIR}/cuadernu_studio_ffi_test.cpp")
        target_link_libraries(cuadernu_studio_ffi_test PRIVATE casio_native)
        target_compile_definitions(cuadernu_studio_ffi_test PRIVATE _CRT_SECURE_NO_WARNINGS)
        if(MSVC)
            target_compile_options(cuadernu_studio_ffi_test PRIVATE /O2 /EHsc /utf-8)
        endif()
        add_test(NAME cuadernu_studio_ffi_test COMMAND cuadernu_studio_ffi_test)

        if(EXISTS "${STUDIO_ROOT}/tinta_core/api.cpp" AND EXISTS "${CORE_TEST_DIR}/ios_bridge_coexistence_test.cpp")
            file(GLOB TINTA_CORE_SOURCES "${STUDIO_ROOT}/tinta_core/*.cpp")
            add_executable(ios_bridge_coexistence_test
                "${CORE_TEST_DIR}/ios_bridge_coexistence_test.cpp"
                ${TINTA_CORE_SOURCES}
            )
            target_include_directories(ios_bridge_coexistence_test PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/casio_nx/include"
                "${CMAKE_CURRENT_SOURCE_DIR}/casio_ffi"
                "${STUDIO_ROOT}/tinta_core"
                "${STUDIO_ROOT}/c_api"
            )
            target_link_libraries(ios_bridge_coexistence_test PRIVATE casio_native)
            if(ZLIB_FOUND)
                target_link_libraries(ios_bridge_coexistence_test PRIVATE ZLIB::ZLIB)
            else()
                target_link_libraries(ios_bridge_coexistence_test PRIVATE z)
            endif()
            set_target_properties(ios_bridge_coexistence_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
            if(WIN32)
                target_compile_definitions(ios_bridge_coexistence_test PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
            endif()
            if(MSVC)
                target_compile_options(ios_bridge_coexistence_test PRIVATE /O2 /EHsc /utf-8)
            endif()
            add_test(NAME ios_bridge_coexistence_test COMMAND ios_bridge_coexistence_test)
        endif()
    endif()

    add_executable(gen_cg50_warm_state "${CORE_TEST_DIR}/gen_cg50_warm_state.cpp")
    target_link_libraries(gen_cg50_warm_state PRIVATE casio_native)
    target_compile_definitions(gen_cg50_warm_state PRIVATE
        CASIO_ROMS_DIR="${CORE_ROMS_DIR}"
        _CRT_SECURE_NO_WARNINGS
    )
    if(MSVC)
        target_compile_options(gen_cg50_warm_state PRIVATE /O2 /EHsc /utf-8)
    endif()
endif()

if(BUILD_CORE_TESTS OR BUILD_CG50_BATTERY)
    add_library(casio_cg50_capi STATIC
        casio_cg50/c_api/rama-cg50_backend.cpp
        casio_cg50/c_api/cg50_core_c_shim.cpp
    )
    target_include_directories(casio_cg50_capi
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/casio_cg50/c_api"
    )
    target_link_libraries(casio_cg50_capi PUBLIC casio_cg50)
    if(WIN32)
        target_compile_definitions(casio_cg50_capi PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(casio_cg50_capi PRIVATE /O2 /EHsc /utf-8)
    else()
        target_compile_options(casio_cg50_capi PRIVATE -O3)
    endif()
endif()

if(BUILD_CORE_TESTS)
    add_executable(cg50_c_api_contract_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core/cg50_core_c_api_contract_test.cpp")
    target_link_libraries(cg50_c_api_contract_test PRIVATE casio_cg50_capi)
    if(WIN32)
        target_compile_definitions(cg50_c_api_contract_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_c_api_contract_test PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_c_api_contract_test COMMAND cg50_c_api_contract_test)

    add_executable(cg50_flash_cfi_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core/cg50_flash_cfi_test.cpp")
    target_link_libraries(cg50_flash_cfi_test PRIVATE casio_cg50)
    if(MSVC)
        target_compile_options(cg50_flash_cfi_test PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_flash_cfi_test COMMAND cg50_flash_cfi_test)

    add_executable(cg50_sh4a_isa_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core/cg50_sh4a_isa_test.cpp")
    target_link_libraries(cg50_sh4a_isa_test PRIVATE casio_cg50)
    if(MSVC)
        target_compile_options(cg50_sh4a_isa_test PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_sh4a_isa_test COMMAND cg50_sh4a_isa_test)

    add_executable(cg50_sh7305_hw_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core/cg50_sh7305_hw_test.cpp")
    target_link_libraries(cg50_sh7305_hw_test PRIVATE casio_cg50)
    if(MSVC)
        target_compile_options(cg50_sh7305_hw_test PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_sh7305_hw_test COMMAND cg50_sh7305_hw_test)

    add_executable(cg50_usb_smem_qr_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core/cg50_usb_smem_qr_test.cpp")
    target_link_libraries(cg50_usb_smem_qr_test PRIVATE casio_native casio_cg50)
    target_compile_definitions(cg50_usb_smem_qr_test PRIVATE
        CASIO_ROMS_DIR="${CORE_ROMS_DIR}"
        _CRT_SECURE_NO_WARNINGS
    )
    if(MSVC)
        target_compile_options(cg50_usb_smem_qr_test PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_usb_smem_qr_test COMMAND cg50_usb_smem_qr_test)

    add_executable(cg50_thread_isolation_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core/cg50_thread_isolation_test.cpp")
    target_link_libraries(cg50_thread_isolation_test PRIVATE casio_cg50)
    target_compile_definitions(cg50_thread_isolation_test PRIVATE
        CASIO_ROMS_DIR="${CORE_ROMS_DIR}"
        _CRT_SECURE_NO_WARNINGS
    )
    if(MSVC)
        target_compile_options(cg50_thread_isolation_test PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_thread_isolation_test COMMAND cg50_thread_isolation_test)

    add_executable(cg50_hw_silicon_100_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core/cg50_hw_silicon_100_test.cpp")
    target_link_libraries(cg50_hw_silicon_100_test PRIVATE casio_cg50)
    if(MSVC)
        target_compile_options(cg50_hw_silicon_100_test PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_hw_silicon_100_test COMMAND cg50_hw_silicon_100_test)

    add_executable(cg50_dynamic_coverage_boost_test "${CMAKE_CURRENT_SOURCE_DIR}/../tests/core/cg50_dynamic_coverage_boost_test.cpp")
    target_link_libraries(cg50_dynamic_coverage_boost_test PRIVATE casio_cg50)
    if(MSVC)
        target_compile_options(cg50_dynamic_coverage_boost_test PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_dynamic_coverage_boost_test COMMAND cg50_dynamic_coverage_boost_test)
endif()

if(BUILD_CG50_BATTERY)
    add_executable(cg50_battery "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_battery/cg50_battery.cpp")
    target_link_libraries(cg50_battery PRIVATE casio_cg50_capi)
    if(WIN32)
        target_compile_definitions(cg50_battery PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_battery PRIVATE /O2 /EHsc /utf-8)
    endif()

    add_executable(cg50_apps_probe "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_battery/cg50_apps_probe.cpp")
    target_link_libraries(cg50_apps_probe PRIVATE casio_cg50_capi)
    if(WIN32)
        target_compile_definitions(cg50_apps_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_apps_probe PRIVATE /O2 /EHsc /utf-8)
    endif()

    # Phase 4 & Phase 6 Extreme Validation, Hardware Torture & Fuzzer runners
    add_executable(cg50_hw_torture_runner "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/stress/cg50_hw_torture_runner.cpp")
    target_link_libraries(cg50_hw_torture_runner PRIVATE casio_cg50)
    if(WIN32)
        target_compile_definitions(cg50_hw_torture_runner PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_hw_torture_runner PRIVATE /O2 /EHsc /utf-8)
    endif()
    if(BUILD_CORE_TESTS)
        add_test(NAME cg50_hw_torture_ci_smoke COMMAND cg50_hw_torture_runner --profile ci-smoke --out "${CMAKE_CURRENT_BINARY_DIR}/torture_smoke")
    endif()

    add_executable(cg50_validation_runner "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/cg50_validation_runner.cpp")
    target_link_libraries(cg50_validation_runner PRIVATE casio_cg50)
    if(WIN32)
        target_compile_definitions(cg50_validation_runner PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_validation_runner PRIVATE /O2 /EHsc /utf-8)
    endif()

    add_executable(cg50_fuzzer "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/fuzzing/cg50_fuzzer.cpp")
    target_link_libraries(cg50_fuzzer PRIVATE casio_cg50)
    if(WIN32)
        target_compile_definitions(cg50_fuzzer PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_fuzzer PRIVATE /O2 /EHsc /utf-8)
    endif()

    add_executable(cg50_graphics_compute_benchmarks "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/graphics/cg50_graphics_compute_benchmarks.cpp")
    target_link_libraries(cg50_graphics_compute_benchmarks PRIVATE casio_cg50)
    if(WIN32)
        target_compile_definitions(cg50_graphics_compute_benchmarks PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_graphics_compute_benchmarks PRIVATE /O2 /EHsc /utf-8)
    endif()

    add_executable(cg50_perf_benchmark "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/stress/cg50_perf_benchmark.cpp")
    target_link_libraries(cg50_perf_benchmark PRIVATE casio_cg50)
    if(WIN32)
        target_compile_definitions(cg50_perf_benchmark PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_link_libraries(cg50_perf_benchmark PRIVATE psapi)
    endif()
    if(MSVC)
        target_compile_options(cg50_perf_benchmark PRIVATE /O2 /EHsc /utf-8)
    endif()

    add_executable(cg50_parametric_scale_100k "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/cg50_parametric_scale_100k.cpp")
    target_link_libraries(cg50_parametric_scale_100k PRIVATE casio_cg50)
    if(WIN32)
        target_compile_definitions(cg50_parametric_scale_100k PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_parametric_scale_100k PRIVATE /O2 /EHsc /utf-8)
    endif()
endif()

if(BUILD_CORE_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/hardware/cg50_cpu_dsp_cert_lab.cpp")
    add_executable(cg50_cpu_dsp_cert_lab "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/hardware/cg50_cpu_dsp_cert_lab.cpp")
    target_link_libraries(cg50_cpu_dsp_cert_lab PRIVATE casio_cg50)
    if(WIN32)
        target_compile_definitions(cg50_cpu_dsp_cert_lab PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_cpu_dsp_cert_lab PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_cpu_dsp_cert_lab_quick COMMAND cg50_cpu_dsp_cert_lab --quick --out "${CMAKE_CURRENT_BINARY_DIR}/cpu_dsp_quick")

    add_executable(cg50_register_cert_lab "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/hardware/cg50_register_cert_lab.cpp")
    target_link_libraries(cg50_register_cert_lab PRIVATE casio_cg50)
    if(WIN32)
        target_compile_definitions(cg50_register_cert_lab PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_register_cert_lab PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_register_cert_lab COMMAND cg50_register_cert_lab --out "${CMAKE_CURRENT_BINARY_DIR}/reg_cert_out")

    add_executable(cg50_combined_hw_stress "${CMAKE_CURRENT_SOURCE_DIR}/../tests/cg50_validation/hardware/cg50_combined_hw_stress.cpp")
    target_link_libraries(cg50_combined_hw_stress PRIVATE casio_cg50)
    if(WIN32)
        target_compile_definitions(cg50_combined_hw_stress PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    if(MSVC)
        target_compile_options(cg50_combined_hw_stress PRIVATE /O2 /EHsc /utf-8)
    endif()
    add_test(NAME cg50_combined_hw_stress_ci COMMAND cg50_combined_hw_stress --seqs 16000 --threads 4 --out "${CMAKE_CURRENT_BINARY_DIR}/stress_ci_out")
endif()
