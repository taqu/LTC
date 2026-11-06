# Phase 3 — LTC LUT Grid Generation and CPU Parallelization

## Project: LTC LUT Generator

### Objective

Extend the single-point LTC fitting engine developed in Phase 2 into a complete two-dimensional LUT generation system.

Phase 3 must implement:

- Two-dimensional LUT parameter grids.
- Reference-compatible roughness and view-angle mapping.
- Complete GGX LTC table generation.
- Reference-compatible fitting traversal and initialization.
- Deterministic CPU execution.
- Dependency-aware CPU parallelization.
- Structured storage of full-precision fitting results.
- Checkpointing and resumable generation.
- Per-cell fitting diagnostics.
- LUT-level numerical validation.
- CLI integration for LUT generation.

The primary reference implementation is:

https://github.com/selfshadow/ltc_code

Relevant reference files:

- `fit/fitLTC.cpp`
- `fit/LTC.h`
- `fit/brdf_ggx.h`
- `fit/nelder_mead.h`

The long-term goal is to generate DDS textures for real-time area-light LTC evaluation, including:

- `GGX_LTCMat.dds`
- `GGX_LTCAmp.dds`

**Phase 3 must generate the underlying LTC coefficient tables, but it must not implement DDS writing or Unreal Engine-specific texture packing. Those features belong to Phase 4.**

---

## 1. Existing Project Integration

The project has completed the following stages.

### Phase 0

- C++20 and CMake infrastructure.
- Standalone CLI application.
- DDS metadata inspection.
- DDS pixel decoding.
- Automated tests.

### Phase 1

- Generic BRDF interface.
- Isotropic GGX implementation.
- GGX direction sampling and PDF evaluation.
- LTC distribution representation.
- LTC sampling and evaluation.
- Mathematical validation.

### Phase 2

- Deterministic midpoint sampling.
- BRDF magnitude integration.
- Average BRDF direction estimation.
- Schlick Fresnel coefficient integration.
- MIS-based fitting error.
- Nelder–Mead optimization.
- Single-point LTC fitting.
- Fitting diagnostics.
- `ltcgen fit` command.

Before implementing Phase 3:

1. Inspect the existing repository.
2. Identify the actual interfaces provided by Phase 2.
3. Review the reference LUT traversal algorithm.
4. Confirm that fitting results contain all information needed to reconstruct an LTC.
5. Preserve all previous functionality.
6. Reuse existing numerical methods without duplicating their implementation.

Do not rewrite the fitting engine unless a correctness issue or essential integration limitation is identified.

If an existing API requires extension, make the smallest necessary change and add regression tests.

---

## 2. Architectural Requirements

Introduce a LUT generation layer between the fitting engine and future exporters.

The intended architecture is:

```text
                 CLI
                  |
                  v
             LUTGenerator
                  |
         +--------+---------+
         |                  |
         v                  v
    ParameterGrid       JobScheduler
         |                  |
         +--------+---------+
                  |
                  v
              LTCFitter
                  |
         +--------+---------+
         |                  |
         v                  v
       IBRDF           LTC / Optimizer
                  |
                  v
               LUTData
                  |
         +--------+---------+
         |                  |
         v                  v
      Validation       Checkpoint I/O

Future Phase 4:

               LUTData
                  |
                  v
             LUTPacking
                  |
                  v
              DDSWriter
```

### Architectural Constraints

- `LUTGenerator` must depend on the generic `IBRDF` interface.
- `LUTGenerator` must not depend directly on the concrete GGX class.
- The fitting engine must not know LUT dimensions.
- The numerical optimizer must not know LUT cell coordinates.
- The LUT data model must not depend on DDS file formats.
- The scheduler must not contain BRDF-specific equations.
- Checkpoint serialization must remain separate from LTC mathematics.
- The CLI must not contain fitting traversal logic.

Maintain clear module responsibilities.

---

## 3. Project Structure

Extend the existing project approximately as follows:

```text
include/ltc/
├── LUT.h
├── ParameterGrid.h
├── LUTGenerator.h
├── GenerationConfig.h
├── GenerationProgress.h
├── JobScheduler.h
└── Checkpoint.h

src/
├── lut/
│   ├── ParameterGrid.cpp
│   ├── LUTGenerator.cpp
│   ├── JobScheduler.cpp
│   ├── Checkpoint.cpp
│   └── LUTValidation.cpp
└── cli/
    └── main.cpp

tests/
├── ParameterGridTests.cpp
├── LUTGeneratorTests.cpp
├── LUTInitializationTests.cpp
├── ParallelGenerationTests.cpp
├── CheckpointTests.cpp
└── LUTValidationTests.cpp

docs/
├── lut-generation.md
├── parallel-generation.md
└── checkpoint-format.md
```

Adapt the structure to existing conventions.

Do not create unused placeholder files.

---

## 4. LUT Parameter Grid

Implement a reusable two-dimensional parameter grid.

The initial grid represents:

- Perceptual roughness.
- View angle.

The reference resolution is:

```text
Width:  64
Height: 64
Cells:  4096
```

The grid must support configurable dimensions.

### 4.1 Roughness Mapping

For roughness index `a`:

\[
r=\frac{a}{W-1}
\]

The GGX alpha parameter is:

\[
\alpha=\max(r^2,10^{-5})
\]

where:

- `r` is perceptual roughness.
- `alpha` is the microfacet roughness parameter.
- `W` is the number of roughness samples.

The minimum alpha must remain configurable outside strict reference behavior.

The default Reference profile must use:

```cpp
constexpr double MinAlpha = 1e-5;
```

Do not confuse this value with the minimum LTC matrix scale used by the fitting optimizer.

### 4.2 View-Angle Mapping

For view-angle index `t`:

\[
x=\frac{t}{H-1}
\]

\[
\cos\theta=1-x^2
\]

\[
\theta=\min(\arccos(\cos\theta),1.57)
\]

The view direction is:

\[
V=(\sin\theta,0,\cos\theta)
\]

This follows the reference parameterization.

Do not replace the 1.57-radian grazing-angle limit with exactly \(\pi/2\) in Reference mode.

The exact boundary behavior matters when comparing with reference coefficients.

### 4.3 Grid Orientation

Use:

```text
X axis: Roughness
Y axis: View angle
```

The internal cell index must be:

```cpp
index = roughnessIndex
      + viewIndex * width;
```

This follows the logical storage convention used by the reference implementation.

Distinguish:

- Logical LUT coordinates.
- Fitting traversal order.
- Memory storage order.
- Future DDS row order.

These must not be conflated.

### 4.4 Parameter Grid API

Suggested interface:

```cpp
struct LUTCoordinates
{
    uint32_t roughnessIndex;
    uint32_t viewIndex;
};

struct LUTParameters
{
    double perceptualRoughness;
    double alpha;
    double ndotv;
    double theta;
    Vec3 viewDirection;
};

class ParameterGrid
{
public:
    ParameterGrid(
        uint32_t width,
        uint32_t height);

    LUTParameters evaluate(
        LUTCoordinates coordinates) const;

    uint32_t width() const;
    uint32_t height() const;

    size_t index(
        LUTCoordinates coordinates) const;
};
```

Support the reference parameterization initially.

Future BRDFs may require different parameter mappings, so do not embed GGX-specific mappings directly into the scheduler.

### Grid Validation

Test:

- First and last roughness samples.
- First and last view-angle samples.
- Alpha floor.
- Grazing-angle clamp.
- Monotonicity.
- Correct indexing.
- Non-square grids.
- Invalid dimensions.
- Integer overflow in total cell count.

Require both dimensions to be at least two for the initial parameterization.

---

## 5. LUT Data Model

Implement an in-memory representation of the complete fitting table.

The table must retain full-precision results before GPU-oriented packing.

### Suggested Structures

```cpp
struct LUTCell
{
    LTCParameters parameters;

    Mat3 matrix;
    Mat3 inverseMatrix;

    double magnitude = 0.0;
    double fresnelCoefficient = 0.0;

    bool hasFresnelCoefficient = false;

    double initialError = 0.0;
    double finalError = 0.0;

    uint32_t iterations = 0;
    uint64_t evaluations = 0;

    FitStatus status = FitStatus::InvalidInput;

    bool completed = false;
};

struct LUTMetadata
{
    std::string brdfName;

    uint32_t width = 0;
    uint32_t height = 0;

    FittingProfile fittingProfile;

    uint32_t sampleResolution = 32;
    uint32_t maxIterations = 100;
};

class LUTData
{
public:
    LUTCell& at(
        uint32_t roughnessIndex,
        uint32_t viewIndex);

    const LUTCell& at(
        uint32_t roughnessIndex,
        uint32_t viewIndex) const;

    uint32_t width() const;
    uint32_t height() const;
};
```

The exact types may be adapted to the existing project.

### Requirements

- Preserve the LTC parameters and orientation basis.
- Preserve the full forward matrix.
- Preserve the inverse matrix or provide an exact reconstruction path.
- Preserve magnitude and Fresnel coefficients.
- Preserve fitting errors.
- Preserve optimizer status.
- Preserve iteration and objective evaluation counts.
- Distinguish completed cells from uninitialized cells.
- Avoid exposing partially written cells to other threads.

Store sufficient information to reproduce Phase 4 output without rerunning the fitter.

Do not quantize coefficients to FP16 in this phase.

Do not apply Unreal Engine-specific matrix normalization or channel packing.

---

## 6. Reference-Compatible Fitting Traversal

This is one of the most important requirements of Phase 3.

The reference implementation does not fit each LUT cell independently.

It reuses previous fitting results to initialize subsequent cells.

### 6.1 Traversal Order

Process roughness from highest to lowest:

```cpp
for (int a = width - 1; a >= 0; --a)
{
    for (int t = 0; t < height; ++t)
    {
        // Fit one LTC cell.
    }
}
```

For each roughness level, process view angles from normal incidence toward grazing incidence.

### 6.2 First Cell

For the first cell:

```text
roughnessIndex = width - 1
viewIndex = 0
```

Use:

```text
m11 = 1
m22 = 1
m13 = 0
```

The LTC basis must be the identity basis.

This is the initial state of the reference fitting traversal.

### 6.3 Normal-Incidence Initialization

For each subsequent roughness level at `viewIndex = 0`:

- Use the fitted normal-incidence scale from the previous, higher-roughness row.
- Use an identity basis.
- Set `m13 = 0`.
- Apply the isotropic constraint.

Conceptually:

```cpp
initial.m11 = previousNormalIncidence.m11;
initial.m22 = previousNormalIncidence.m22;
initial.m13 = 0.0;
```

If the stored matrix is used instead of LTC parameters, ensure the extracted scales are equivalent to the reference behavior.

The original reference extracts the required diagonal coefficients from the previously stored transformation matrix.

### 6.4 Subsequent View-Angle Initialization

For `viewIndex > 0`:

- Reuse the fitted LTC shape parameters from the immediately preceding view-angle cell.
- Compute the average BRDF direction for the current cell.
- Reconstruct the LTC orientation basis from the current average direction.
- Apply the reused shape parameters to the new basis.
- Run the fitter.

Do not reuse the preceding cell's complete matrix without updating its orientation.

The shape parameters and basis have different responsibilities.

### 6.5 Reconstructing the Orientation

For the current average direction:

```cpp
Vec3 Z = averageDirection;
Vec3 X = Vec3(Z.z, 0.0, -Z.x);
Vec3 Y = Vec3(0.0, 1.0, 0.0);
```

Use the reference-compatible basis construction.

The average direction is expected to lie in the XZ plane for isotropic BRDFs.

Do not introduce arbitrary rotations or basis modifications in Reference mode.

### 6.6 Integration Ownership

Phase 2 already calculates the average direction, magnitude, and Fresnel coefficient.

Avoid computing these quantities twice merely because the generator needs the current orientation.

If the existing fitter API makes this necessary, introduce a minimal API extension, such as:

```cpp
BRDFIntegrationResult integrate(...);

FittingResult fit(
    ...,
    const BRDFIntegrationResult& integrated,
    const LTCInitialGuess& initial);
```

Alternatively, allow the fitter to accept an initializer callback that receives the integrated BRDF terms.

The objective is to preserve the Phase 2 numerical algorithm while making the reference initialization sequence possible.

Do not duplicate BRDF integration code inside the LUT generator.

### 6.7 Sequential Reference Mode

Implement a fully sequential generation path first.

It must:

- Follow the exact reference traversal dependencies.
- Use the configured fitting profile.
- Preserve deterministic initialization.
- Produce reproducible results.
- Record all cell diagnostics.

Do not parallelize the initial implementation before validating its correctness.

---

## 7. CPU Parallelization

After sequential generation is correct, implement dependency-aware CPU parallelization.

### Important Constraint

Adjacent cells are not independent under reference initialization.

A naïve thread pool processing all 4096 cells concurrently would change the initial guesses and potentially the fitting results.

Do not implement unrestricted cell-level parallelism.

### 7.1 Dependency Model

For each cell `(a, t)`:

At normal incidence (`t = 0`):

```text
(a, 0) depends on (a + 1, 0)
```

Except for the highest roughness row.

At non-normal incidence (`t > 0`):

```text
(a, t) depends on (a, t - 1)
```

The resulting dependency graph is acyclic.

### 7.2 Recommended Parallel Strategy

Use a pipeline of roughness rows.

The scheduler should:

1. Fit the highest-roughness normal-incidence cell.
2. Continue that row toward grazing incidence.
3. Once the normal-incidence cell is complete, allow the next roughness row to begin.
4. Each row maintains its own sequential view-angle fitting.
5. Multiple roughness rows may execute concurrently after their respective initial dependencies are satisfied.

Conceptually:

```text
Roughness       View angle
                 0    1    2    3    4

Highest         A -> B -> C -> D -> E
                 |
                 v
Next            F -> G -> H -> I -> J
                 |
                 v
Next            K -> L -> M -> N -> O
                 |
                 v
Lowest          P -> Q -> R -> S -> T
```

A cell may execute only after its dependency is finalized.

The scheduler must not allow a cell to read an incomplete predecessor.

### 7.3 Thread Safety

Requirements:

- Each worker owns its local fitting state.
- The BRDF interface must support concurrent read-only evaluation.
- The optimizer must not use mutable global state.
- The scheduler must synchronize dependent cell publication.
- Different threads must not modify the same LUT cell.
- Progress reporting must be thread-safe.
- Checkpoint writing must not race with cell updates.
- No global random generator.

Use standard C++ synchronization primitives where practical:

- `std::thread` or `std::jthread`
- `std::mutex`
- `std::condition_variable`
- `std::atomic`

Avoid unnecessary external concurrency dependencies.

### 7.4 Determinism

Scheduling order must not alter the numerical inputs to any fitted cell.

Each cell must receive the same initial parameters regardless of worker count.

The goal is equivalent numerical results between:

```bash
ltcgen generate --threads 1
```

and:

```bash
ltcgen generate --threads 8
```

Under the same executable and floating-point environment, require identical results for deterministic fitting operations where practical.

If exact equality cannot be guaranteed, document the reason and verify differences against strict tolerances.

Do not accept thread-dependent random initialization or reduction ordering.

### 7.5 Thread Count

Support:

```text
--threads 1
--threads 4
--threads 8
--threads auto
```

`auto` may use `std::thread::hardware_concurrency()`.

Handle zero or unavailable hardware-concurrency results safely.

Do not create an unreasonable number of worker threads.

### 7.6 Failure Handling

If a cell fails:

- Record its failure status.
- Do not silently mark it as converged.
- Do not expose an invalid matrix as a valid initialization.
- Apply a documented policy to dependent cells.
- Avoid deadlocks.
- Ensure worker threads terminate cleanly.

Distinguish between:

- Converged.
- Reached maximum iterations with a valid result.
- Numerical failure.
- Invalid input.
- Not yet processed.

A valid best-effort result may be used as an initializer according to an explicit policy.

A failed or invalid result must not silently propagate through the grid.

---

## 8. Generation Configuration

Introduce a configuration object.

Suggested structure:

```cpp
struct GenerationConfig
{
    uint32_t width = 64;
    uint32_t height = 64;

    uint32_t threads = 1;

    FittingConfig fitting;

    bool enableCheckpoint = false;

    uint32_t checkpointInterval = 64;
};
```

The default profile should emphasize reference compatibility and reproducibility.

### Configuration Requirements

- Validate dimensions.
- Validate thread count.
- Validate fitting parameters.
- Validate total cell count and memory requirements.
- Support explicit configuration overrides.
- Preserve all generation settings in metadata.

The generator must not silently replace invalid options with defaults.

### Reference Profile

Use the established reference grid and fitting conventions.

Default to:

```text
Width: 64
Height: 64
Samples: 32
Iterations: 100
Profile: Reference
```

Allow reduced grid sizes for development and testing.

However, do not claim a reduced-resolution LUT is numerically equivalent to the 64×64 reference grid.

---

## 9. LUT Generator API

Implement a reusable high-level generation API.

Suggested interface:

```cpp
class LUTGenerator
{
public:
    LUTData generate(
        const IBRDF& brdf,
        const GenerationConfig& config,
        GenerationProgress* progress = nullptr) const;
};
```

Adapt error handling to established repository conventions.

### Responsibilities

The generator must:

1. Validate configuration.
2. Create the parameter grid.
3. Allocate LUT storage.
4. Establish fitting dependencies.
5. Schedule cell fitting.
6. Provide reference-compatible initial guesses.
7. Call the Phase 2 fitter.
8. Store the full fitting results.
9. Track progress and failures.
10. Produce a complete LUTData object.

It must not:

- Write DDS files.
- Quantize to FP16.
- Pack coefficients into GPU channels.
- Perform BRDF-specific equations.
- Implement numerical optimization internally.

---

## 10. Checkpointing and Resume

A complete LUT fit may take substantial CPU time.

Implement resumable generation.

### 10.1 Checkpoint Requirements

A checkpoint must preserve:

- Checkpoint format version.
- BRDF identity.
- BRDF implementation or configuration identity.
- Grid dimensions.
- Parameter mapping.
- Fitting profile.
- Fitting sample count.
- Optimizer configuration.
- Numerical precision.
- Completed cell states.
- Full-precision LTC parameters.
- Magnitude and Fresnel coefficients.
- Fit status and diagnostics.

Do not store only GPU-packed coefficients.

### 10.2 Checkpoint Format

Use an explicitly versioned, documented format.

A binary format is recommended for compactness and exact coefficient preservation.

JSON may be used for metadata, but do not require a large JSON document containing all numerical cell data.

Avoid relying on raw in-memory serialization of C++ structures.

Do not serialize compiler-dependent padding, pointer values, or opaque STL internals.

Use fixed-width fields and documented byte ordering.

### 10.3 Atomic Checkpoint Updates

Checkpoint updates must not corrupt a previously valid checkpoint.

Recommended approach:

1. Write a complete checkpoint to a temporary file.
2. Flush and close the file.
3. Validate the written data.
4. Atomically replace the previous checkpoint where supported.

Document platform-specific limitations of atomic replacement.

Use checksums or other integrity checks to detect truncated or corrupted checkpoints.

### 10.4 Resume Validation

On resume:

- Validate checkpoint version.
- Validate BRDF identity and configuration.
- Validate grid dimensions.
- Validate fitting configuration.
- Validate stored cell data.
- Validate dependency completion.

Reject incompatible checkpoints with useful diagnostics.

Do not silently restart from scratch if the user explicitly requested resume.

### 10.5 Dependency-Aware Resume

When resuming, only trust completed cells whose recorded predecessors are valid.

A cell may be reused if:

- Its fitting result is complete.
- Its result data is valid.
- Its initialization dependencies are satisfied.
- Its configuration matches the requested generation.

Do not skip a required predecessor merely because a later cell has a serialized result.

### 10.6 Checkpoint Frequency

Support configurable checkpoint intervals.

For example:

```bash
ltcgen generate \
    --checkpoint ./ggx.checkpoint \
    --checkpoint-interval 64
```

Checkpointing should have minimal impact on normal fitting performance.

Do not write a complete checkpoint after every objective evaluation.

Save only finalized cell results.

---

## 11. Progress Reporting

Provide useful progress information for long-running LUT generation.

Recommended fields:

- Total cells.
- Completed cells.
- Successfully converged cells.
- Maximum-iteration cells.
- Failed cells.
- Active workers.
- Elapsed time.
- Average cell duration.

Example:

```text
BRDF: GGX
Resolution: 64x64
Profile: Reference
Threads: 8

Completed: 1536 / 4096
Progress: 37.5%

Converged: 1501
MaxIterations: 35
Failed: 0
```

Values must reflect actual execution.

Progress reporting must not affect numerical results.

Avoid printing every optimization iteration or every cell unless verbose mode is enabled.

Provide a machine-readable summary suitable for CI usage.

---

## 12. CLI Integration

Add the `generate` command.

### Basic Generation

```bash
ltcgen generate \
    --brdf ggx \
    --width 64 \
    --height 64 \
    --profile reference
```

### Multithreaded Generation

```bash
ltcgen generate \
    --brdf ggx \
    --profile reference \
    --threads 8
```

### Development-Sized Generation

```bash
ltcgen generate \
    --brdf ggx \
    --width 8 \
    --height 8 \
    --samples 16 \
    --max-iterations 50
```

### Checkpointed Generation

```bash
ltcgen generate \
    --brdf ggx \
    --checkpoint ./ggx.checkpoint \
    --checkpoint-interval 64
```

### Resume

```bash
ltcgen generate \
    --resume ./ggx.checkpoint
```

### JSON Summary

```bash
ltcgen generate \
    --brdf ggx \
    --width 8 \
    --height 8 \
    --json
```

### CLI Output Policy

Because DDS writing belongs to Phase 4, use an explicitly named intermediate output format.

For example:

```bash
ltcgen generate \
    --brdf ggx \
    --output ./ggx.ltcbin
```

The intermediate file must store full-precision LUT data.

Do not give the intermediate file a `.dds` extension.

Do not create a fake or incomplete DDS writer.

Document the intermediate format and ensure the generated data can be loaded again by the program.

If checkpoint files and completed intermediate files use the same underlying container format, distinguish their purpose and completion state explicitly.

### Existing Commands

Preserve:

- `ltcgen inspect`
- `ltcgen fit`
- `ltcgen --version`
- `ltcgen --help`

Do not break Phase 0 or Phase 2 CLI behavior.

---

## 13. LUT-Level Validation

Introduce validation of the complete generated table.

### 13.1 Structural Validation

Check:

- Expected number of cells.
- Correct index mapping.
- Completed cell state.
- Valid matrices.
- Finite coefficients.
- Valid magnitude values.
- Valid Fresnel coefficients when supported.
- Consistent metadata.
- Valid fit statuses.

### 13.2 Parameter Continuity

Neighboring LUT cells should generally vary smoothly.

Measure differences in:

- `m11`
- `m22`
- `m13`
- Matrix elements.
- Magnitude.
- Fresnel coefficient.

Report large discontinuities.

Do not automatically smooth the values in Reference mode.

A discontinuity may indicate:

- Poor convergence.
- Incorrect initialization.
- Wrong grid mapping.
- Invalid matrix reconstruction.
- A genuinely difficult fitting region.

### 13.3 Fitting Error Statistics

Aggregate:

- Minimum error.
- Maximum error.
- Mean error.
- Median error.
- RMS error.
- Convergence count.
- Maximum-iteration count.
- Failure count.

Exclude invalid or non-finite errors from finite aggregates and report their counts separately.

### 13.4 Independent Numerical Error

Evaluate selected cells using a higher sampling resolution than the fitting pass.

For example:

```text
Fitting samples:    32 x 32
Validation samples: 128 x 128
```

Include representative cases from:

- Normal incidence.
- Intermediate view angles.
- Grazing incidence.
- High roughness.
- Low roughness.

Do not confuse optimizer convergence with approximation quality.

Store independent validation errors separately from fitting objective errors.

### 13.5 Reference Comparison

Where reference data is available, compare generated GGX LUT coefficients with the original LTC fitting reference.

The comparison must account for:

- Reference matrix conventions.
- Roughness mapping.
- View-angle mapping.
- Floating-point precision.
- Optimization behavior.
- Traversal initialization.

If the available reference data contains only packed GPU coefficients, defer the complete packed-texture comparison to Phase 4.

Do not compare incompatible matrix representations directly.

Do not invent reference values.

---

## 14. Automated Tests

Extend GoogleTest and CTest coverage.

### Parameter Grid Tests

Verify exact mapping at:

- First cell.
- Last cell.
- Normal incidence.
- Grazing incidence.
- Alpha floor.
- Intermediate roughness.
- Intermediate view angle.

Test rectangular grids.

### Initialization Tests

Verify:

- Initial identity LTC.
- Normal-incidence initialization from the preceding roughness level.
- Subsequent view-angle initialization from the preceding cell.
- Correct reconstruction of the LTC orientation.
- Correct isotropic constraint.
- Invalid predecessor handling.

Use a mock fitter or test instrumentation where necessary to inspect initial guesses without running expensive optimization.

### Generation Tests

Generate small LUTs such as:

```text
2x2
4x4
8x8
```

Verify:

- Correct cell count.
- Correct fitting order and dependencies.
- Complete result storage.
- Finite matrix values.
- Deterministic execution.
- Correct failure propagation.

Do not require a full 64×64 fit in ordinary unit tests.

### Parallel Generation Tests

Compare sequential and multithreaded generation.

Test:

- One thread.
- Two threads.
- Four threads.
- More threads than roughness rows.

Verify:

- No missing cells.
- No duplicate fitting.
- No incomplete dependency reads.
- Equivalent initialization.
- Equivalent numerical results.
- No deadlocks.
- Correct worker shutdown after failures.

Use ThreadSanitizer where supported.

Do not claim ThreadSanitizer validation on platforms where it was not actually executed.

### Checkpoint Tests

Test:

- Save and load.
- Exact full-precision coefficient preservation.
- Interrupted generation.
- Resume from partial completion.
- Incompatible configuration.
- Corrupted checkpoint.
- Truncated checkpoint.
- Unsupported checkpoint version.
- Incorrect dependency state.
- Resume with a different thread count.

Compare resumed output against uninterrupted generation.

### Performance Tests

Measure:

- Sequential generation time.
- Multithreaded generation time.
- Average fit time per cell.
- Checkpoint overhead.
- Peak memory usage where measurable.

Keep performance benchmarks separate from correctness unit tests.

Do not enforce fragile machine-dependent timing thresholds in CI.

---

## 15. Performance Requirements

The implementation should support efficient CPU generation without compromising correctness.

### General Requirements

- Avoid unnecessary allocations per cell.
- Reuse fitting configuration.
- Avoid repeated construction of invariant objects.
- Keep worker-local fitting state.
- Avoid redundant BRDF integration.
- Minimize synchronization overhead.
- Avoid excessive progress logging.
- Do not introduce global mutable numerical state.

### Reference Mode

Prioritize compatibility and deterministic behavior.

Avoid optimizations that change the fitted objective or numerical inputs.

### Parallel Mode

Parallelization must not change fitting dependencies.

Benchmark thread counts such as:

```text
1
2
4
8
```

Report actual speedups rather than assuming linear scaling.

The dependency graph limits available parallelism, especially at the beginning of generation.

Do not introduce GPU compute or external distributed processing.

---

## 16. Documentation

Update the following documentation.

### docs/lut-generation.md

Describe:

- LUT parameter mapping.
- Grid dimensions.
- Grid indexing.
- Reference traversal.
- Initialization dependencies.
- LUT data model.
- Generation configuration.
- Error handling.
- Intermediate output format.

### docs/parallel-generation.md

Describe:

- Dependency graph.
- Worker scheduling.
- Synchronization strategy.
- Determinism requirements.
- Failure propagation.
- Thread-safety assumptions.
- Performance measurements.

### docs/checkpoint-format.md

Describe:

- Checkpoint version.
- Metadata.
- Data layout.
- Floating-point representation.
- Integrity checks.
- Resume rules.
- Atomic update behavior.
- Compatibility limitations.

### README.md

Add:

- LUT generation examples.
- Single-threaded and multithreaded usage.
- Checkpoint and resume examples.
- Supported BRDFs.
- Current limitations.

Explicitly state that DDS output is planned for Phase 4.

Do not advertise DDS generation before it exists.

---

## 17. Scope Restrictions

Phase 3 must not implement:

- DDS writing.
- DDS texture packing.
- FP16 coefficient quantization.
- Unreal Engine-specific texture output.
- Area-light polygon integration.
- New BRDF mathematical models.
- Alternative fitting optimizers.
- GPU compute.
- Distributed fitting.
- GUI.
- Path-tracing features.
- Spherical-cap auxiliary LUTs.

Do not introduce optional features that delay the core LUT generator.

Focus on producing correct, reusable, full-precision LTC coefficient tables.

---

## 18. Acceptance Criteria

Phase 3 is complete when:

- [ ] A configurable two-dimensional parameter grid is implemented.
- [ ] Reference roughness mapping is correct.
- [ ] Reference view-angle mapping is correct.
- [ ] LUT indexing and storage are correct.
- [ ] Full-precision LTC coefficient storage is implemented.
- [ ] Reference-compatible sequential traversal is implemented.
- [ ] First-cell initialization is correct.
- [ ] Normal-incidence initialization uses the preceding roughness row.
- [ ] Subsequent view-angle initialization uses the preceding cell.
- [ ] Current-cell average direction is used for LTC orientation.
- [ ] Existing Phase 2 fitting logic is reused.
- [ ] Complete GGX LUT generation works.
- [ ] Dependency-aware CPU parallelization works.
- [ ] Parallel generation preserves deterministic initialization.
- [ ] Configurable worker counts are supported.
- [ ] Per-cell fitting diagnostics are stored.
- [ ] Failed fitting cells are handled explicitly.
- [ ] Progress reporting works.
- [ ] Checkpoints can be written and loaded.
- [ ] Interrupted generation can be resumed correctly.
- [ ] Checkpoint compatibility is validated.
- [ ] Full-precision intermediate LUT output is available.
- [ ] Intermediate LUT data can be loaded again.
- [ ] LUT structural validation works.
- [ ] Fitting-error statistics are available.
- [ ] Representative independent numerical validation works.
- [ ] Automated tests cover grid mapping and fitting dependencies.
- [ ] Parallel generation tests pass.
- [ ] Checkpoint and resume tests pass.
- [ ] Phase 0–2 tests continue to pass.
- [ ] The project builds without Unreal Engine or a GPU SDK.
- [ ] Documentation is updated.
- [ ] DDS output remains deferred to Phase 4.

---

## 19. Implementation Procedure

Implement Phase 3 incrementally in the following order:

1. Inspect the existing project and Phase 2 fitting API.
2. Review the reference LUT traversal implementation.
3. Implement the parameter grid.
4. Add parameter-grid unit tests.
5. Implement the LUT data model.
6. Implement reference-compatible sequential traversal.
7. Add initialization and dependency tests.
8. Generate and validate a small GGX LUT.
9. Generate a full 64×64 GGX LUT sequentially.
10. Record numerical results and fitting diagnostics.
11. Implement the dependency-aware job scheduler.
12. Add parallel generation tests.
13. Compare sequential and parallel results.
14. Implement versioned intermediate LUT serialization.
15. Implement checkpoint saving and loading.
16. Implement resume functionality.
17. Add checkpoint integrity and recovery tests.
18. Implement progress reporting.
19. Add the `ltcgen generate` command.
20. Implement LUT-level structural and numerical validation.
21. Run the full regression test suite.
22. Benchmark sequential and parallel generation.
23. Update documentation.
24. Prepare the Phase 3 implementation report.

Do not implement all functionality in one large change without intermediate validation.

If the existing Phase 2 API cannot support reference-compatible initialization, address that limitation before implementing the parallel scheduler.

### Final Report

After implementation, provide:

1. Summary of implemented functionality.
2. Files created and modified.
3. LUT grid and coordinate conventions.
4. Reference-compatible traversal and initialization behavior.
5. Sequential generation results.
6. CPU parallelization strategy.
7. Sequential versus parallel numerical comparison.
8. Fitting convergence statistics.
9. Checkpoint and resume behavior.
10. LUT validation results.
11. Build and test results.
12. CPU performance measurements.
13. Known numerical or architectural limitations.
14. Deviations from this specification and their rationale.
15. Readiness for Phase 4.

**Expected result: A deterministic, CPU-based LUT generation system capable of fitting and storing a complete 64×64 GGX LTC table, supporting reference-compatible initialization, dependency-aware parallel execution, checkpointing, resumption, and numerical validation without producing DDS textures yet.**
