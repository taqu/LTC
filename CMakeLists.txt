cmake_minimum_required(VERSION 3.21)
project(LTCGenerator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LTC_BUILD_TESTS "Build LTC Generator tests" ON)

include(cmake/CompilerOptions.cmake)
include(FetchContent)

# CLI11
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.4.2
)

# nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(cli11 nlohmann_json)

# ltc_core library
add_library(ltc_core STATIC
    src/core/Version.cpp
    src/dds/DDSReader.cpp
)
target_include_directories(ltc_core PUBLIC include)
ltc_set_compiler_options(ltc_core)

# ltcgen executable
add_executable(ltcgen src/cli/main.cpp)
target_link_libraries(ltcgen PRIVATE ltc_core CLI11::CLI11 nlohmann_json::nlohmann_json)
ltc_set_compiler_options(ltcgen)

if(LTC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
