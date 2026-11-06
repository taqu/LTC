# Architecture

The project has two build targets: a static library, `ltc_core`, which contains all fitting, LUT, DDS, and validation code, and a thin CLI executable, `ltcgen` (`src/cli/main.cpp`), built on CLI11.

## Data Flow

```text
BRDF (IBRDF) ──► LTCFitter / QualityFitter ──► LUTGenerator ──► .ltcbin (Checkpoint)
                                                     │                    │
                                     ParameterGrid, JobScheduler          ▼
                                                                    LUTExporter ──► DDSWriter ──► *.dds
                                                                          │
                                       ValidationRunner ◄─────────────────┘
```

## Components

| Area | Headers (`include/ltc/`) | Responsibility |
|------|--------------------------|----------------|
| BRDFs | `BRDF.h`, `BRDFCapabilities.h`, `GGX.h`, `Beckmann.h`, `DisneyDiffuse.h`, `BRDFRegistry.h` | `IBRDF` interface, built-in models, name → factory registry |
| LTC model | `LTC.h` | LTC matrix (`m11`, `m22`, `m13`), evaluation and sampling |
| Fitting | `BRDFIntegration.h`, `ErrorFunction.h`, `NelderMead.h`, `Fitter.h`, `FittingConfig.h`, `QualityFitting.h`, `AdaptiveSampling.h`, `InitialGuessStrategy.h`, `FittingDiagnostics.h` | BRDF moment integration, MIS error, Reference and Quality fitters |
| LUT | `ParameterGrid.h`, `LUT.h`, `LUTGenerator.h`, `JobScheduler.h`, `GenerationConfig.h`, `GenerationProgress.h`, `Checkpoint.h`, `LUTValidation.h`, `LUTRefinement.h`, `RefinementPolicy.h` | Grid parameterization, multithreaded generation with warm-started initial guesses, `.ltcbin` I/O, summary checks, refinement API |
| Export | `LUTPacking.h`, `LUTExporter.h`, `TextureData.h`, `Float16.h`, `DDSWriter.h` | Matrix/amplitude packing, FP16/FP32 encoding, DDS writing |
| DDS reading | `DDS.h`, `DDSReader.h` | Metadata, pixel, and statistics for `inspect` |
| Validation | `Validation.h`, `ValidationConfig.h`, `ValidationReport.h`, `DistributionValidation.h`, `ReferenceComparison.h`, `AreaLight.h`, `LTCPolygonIntegrator.h`, `NumericalIntegrator.h` | Distribution error, reference texture comparison, area-light integration, JSON reports |
| Benchmarks | `Benchmark.h` | Fit, generation, suite, and scaling benchmarks |

## CLI Subcommands

| Subcommand | Library entry point |
|------------|---------------------|
| `inspect` | `DDSReader` |
| `list-brdfs` | `BRDFRegistry::createDefault()` |
| `fit` | `LTCFitter` |
| `generate` | `LUTGenerator`, `Checkpoint`, `LUTValidator` |
| `export` | `Checkpoint::load`, `LUTExporter` |
| `validate …` | `ValidationRunner` |
| `benchmark …` | `runFitBenchmark`, `runGenerationBenchmark`, `runSuiteBenchmark`, `runScalingBenchmark` |

Output is either human-readable text or JSON. Errors go to stderr, and the process exits with a non-zero code.

## Design Principles

- **No global state.** BRDFs and fitters are stateless and can be called concurrently.
- **Fail fast.** File reads are bounds-checked, and size overflows are detected before allocation.
- **Safe type punning.** File data is decoded with `std::memcpy`, not `reinterpret_cast`.
- **Portable I/O.** File access uses only `std::filesystem` and standard streams, with no platform-specific calls in the library code.
