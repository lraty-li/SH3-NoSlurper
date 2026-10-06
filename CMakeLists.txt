cmake_minimum_required(VERSION 3.24)

project(SH3NoSlurper VERSION 0.3.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "SH3-NoSlurper is Windows-only.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Silent Hill 3 is a 32-bit game. Configure this project for Win32/x86.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(NoSlurper MODULE
    src/dllmain.cpp
    src/patch_core.cpp
)

target_include_directories(NoSlurper PRIVATE src)

set_target_properties(NoSlurper PROPERTIES
    PREFIX ""
    SUFFIX ".asi"
    OUTPUT_NAME "NoSlurper"
)

if(MSVC)
    set_property(TARGET NoSlurper PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(NoSlurper PRIVATE /W4 /permissive- /EHsc)
    target_compile_definitions(NoSlurper PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(NoSlurperTests
        tests/test_patch_core.cpp
        src/patch_core.cpp
    )
    target_include_directories(NoSlurperTests PRIVATE src)

    if(MSVC)
        set_property(TARGET NoSlurperTests PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        target_compile_options(NoSlurperTests PRIVATE /W4 /permissive- /EHsc)
    endif()

    add_test(NAME NoSlurperTests COMMAND NoSlurperTests)
endif()
