cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME ltc)
project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LTC_BUILD_TESTS "Build LTC Generator tests" ON)

include(FetchContent)

# CLI11
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)

# DirectXTex
#FetchContent_Declare(
#    DirectXTex
#    GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
#    GIT_TAG may2026
#)

# GLM — header-only math library for vectors and matrices
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_MakeAvailable(cli11 glm)

set(HEADER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
include_directories(AFTER ${HEADER_ROOT})

set(HEADERS
    "${HEADER_ROOT}/fit/brdf.h"
    "${HEADER_ROOT}/fit/dds.h"
    "${HEADER_ROOT}/fit/float_to_half.h"
    "${HEADER_ROOT}/fit/ltc.h"
    "${HEADER_ROOT}/fit/nelder_mead.h"
)

set(SOURCES
    "${SOURCE_ROOT}/fit/dds.cpp"
    "${SOURCE_ROOT}/fit/brdf.cpp"
    "${SOURCE_ROOT}/fit/ltc.cpp"
    "${SOURCE_ROOT}/fit/nelder_mead.cpp"
    "${SOURCE_ROOT}/main.cpp"
)

source_group("include" FILES ${HEADERS})
source_group("src" FILES ${SOURCES})

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
target_link_libraries(${PROJECT_NAME} PRIVATE CLI11::CLI11 glm::glm)
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /utf-8
        /arch:AVX2
        $<$<CONFIG:Release>:/O2>
        )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Debug>:-g>
        )
endif()

