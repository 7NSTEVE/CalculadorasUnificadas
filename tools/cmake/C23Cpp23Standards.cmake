# ============================================================================
# tools/cmake/C23Cpp23Standards.cmake
# Canonical ISO C23 + ISO C++23 multi-compiler configuration for
# CalculadorasUnificadas (MSVC 19.44+, Clang/clang-cl 21+, GCC 16+, NDK 28+).
# ============================================================================
include_guard(GLOBAL)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_definitions($<$<COMPILE_LANGUAGE:CXX>:_SILENCE_CXX20_U8PATH_DEPRECATION_WARNING>)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:C>:/std:clatest>
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
    )
endif()

function(cu_apply_c23_cpp23 target_name)
    if(NOT TARGET "${target_name}")
        return()
    endif()
    get_target_property(_tgt_type "${target_name}" TYPE)
    if(_tgt_type STREQUAL "INTERFACE_LIBRARY")
        set(_scope INTERFACE)
    else()
        set(_scope PRIVATE)
    endif()

    set_target_properties("${target_name}" PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
    )

    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_definitions("${target_name}" ${_scope}
            $<$<COMPILE_LANGUAGE:CXX>:_SILENCE_CXX20_U8PATH_DEPRECATION_WARNING>
        )
        target_compile_options("${target_name}" ${_scope}
            $<$<COMPILE_LANGUAGE:C>:/std:clatest /utf-8>
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++latest /Zc:__cplusplus /utf-8 /EHsc>
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND MSVC)
        # clang-cl on Windows (full ISO C23 & C++23 with MSVC ABI)
        target_compile_options("${target_name}" ${_scope}
            $<$<COMPILE_LANGUAGE:C>:/std:c23 /utf-8>
            $<$<COMPILE_LANGUAGE:CXX>:/std:c++23 /utf-8 /EHsc>
        )
    else()
        target_compile_options("${target_name}" ${_scope}
            $<$<COMPILE_LANGUAGE:C>:-std=c23>
            $<$<COMPILE_LANGUAGE:CXX>:-std=c++23>
        )
    endif()
endfunction()
