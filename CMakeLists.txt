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

# GLM — header-only math library for vectors and matrices
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_MakeAvailable(cli11 nlohmann_json glm)

# ltc_core library
add_library(ltc_core STATIC
    src/core/Version.cpp
    src/core/Math.cpp
    src/core/Sampling.cpp
    src/dds/DDSReader.cpp
    src/brdf/GGX.cpp
    src/brdf/Beckmann.cpp
    src/brdf/DisneyDiffuse.cpp
    src/brdf/BRDFRegistry.cpp
    src/fitting/LTC.cpp
    src/fitting/BRDFIntegration.cpp
    src/fitting/ErrorFunction.cpp
    src/fitting/NelderMead.cpp
    src/fitting/LTCFitter.cpp
    src/fitting/AdaptiveSampling.cpp
    src/fitting/InitialGuessStrategy.cpp
    src/fitting/FittingDiagnostics.cpp
    src/fitting/MultiStartOptimizer.cpp
    src/fitting/QualityFitter.cpp
    src/lut/LUT.cpp
    src/lut/ParameterGrid.cpp
    src/lut/JobScheduler.cpp
    src/lut/Checkpoint.cpp
    src/lut/LUTValidation.cpp
    src/lut/LUTGenerator.cpp
    src/lut/RefinementPolicy.cpp
    src/lut/LUTRefinement.cpp
    src/benchmark/FittingBenchmark.cpp
    src/benchmark/GenerationBenchmark.cpp
    src/benchmark/BenchmarkReport.cpp
    src/dds/Float16.cpp
    src/dds/DDSWriter.cpp
    src/export/LUTPacking.cpp
    src/export/LUTExporter.cpp
    src/validation/AreaLight.cpp
    src/validation/NumericalIntegrator.cpp
    src/validation/LTCPolygonIntegrator.cpp
    src/validation/DistributionValidation.cpp
    src/validation/ReferenceComparison.cpp
    src/validation/LightingValidation.cpp
    src/validation/ValidationRunner.cpp
    src/validation/ValidationReport.cpp
)
target_include_directories(ltc_core PUBLIC include)
target_link_libraries(ltc_core PUBLIC glm::glm nlohmann_json::nlohmann_json)
ltc_set_compiler_options(ltc_core)

# ltcgen executable
add_executable(ltcgen src/cli/main.cpp)
target_link_libraries(ltcgen PRIVATE ltc_core CLI11::CLI11 nlohmann_json::nlohmann_json)
ltc_set_compiler_options(ltcgen)

if(LTC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
