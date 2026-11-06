FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(ltc_tests
    DDSReaderTests.cpp
    CLITests.cpp
    BRDFTests.cpp
    GGXTests.cpp
    BeckmannTests.cpp
    DisneyDiffuseTests.cpp
    BRDFRegistryTests.cpp
    GenericFittingTests.cpp
    GenericExportTests.cpp
    LTCTests.cpp
    SamplingTests.cpp
    NumericalValidationTests.cpp
    BRDFIntegrationTests.cpp
    ErrorFunctionTests.cpp
    NelderMeadTests.cpp
    LTCFitterTests.cpp
    FittingReferenceTests.cpp
    ParameterGridTests.cpp
    LUTGeneratorTests.cpp
    LUTInitializationTests.cpp
    ParallelGenerationTests.cpp
    CheckpointTests.cpp
    LUTValidationTests.cpp
    Float16EncodingTests.cpp
    DDSWriterTests.cpp
    DDSRoundTripTests.cpp
    LUTPackingTests.cpp
    ExportIntegrationTests.cpp
    NumericalIntegratorTests.cpp
    LTCPolygonIntegratorTests.cpp
    DistributionValidationTests.cpp
    ReferenceComparisonTests.cpp
    LightingValidationTests.cpp
    ValidationRegressionTests.cpp
    AdaptiveSamplingTests.cpp
    MultiStartTests.cpp
    FittingStabilityTests.cpp
    LUTRefinementTests.cpp
    OptimizationRegressionTests.cpp
    PerformanceRegressionTests.cpp
)
target_link_libraries(ltc_tests PRIVATE ltc_core GTest::gtest_main)
target_compile_definitions(ltc_tests PRIVATE
    LTCGEN_EXECUTABLE="$<TARGET_FILE:ltcgen>"
)
add_dependencies(ltc_tests ltcgen)
ltc_set_compiler_options(ltc_tests)

include(GoogleTest)
gtest_discover_tests(ltc_tests)
