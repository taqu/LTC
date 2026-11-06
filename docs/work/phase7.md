# Phase 7 — Fitting Quality, Numerical Stability, and CPU Performance Optimization

## Project: LTC LUT Generator

### Objective

Improve the numerical quality, convergence reliability, and CPU performance of the existing LTC fitting framework without sacrificing reference compatibility or generic BRDF support.

The project currently supports:

- GGX.
- Beckmann.
- Disney/Burley Diffuse.
- Custom BRDF implementations.
- CPU-based LTC fitting.
- Deterministic LUT generation.
- DDS export.
- Reference comparison.
- Numerical area-light validation.

Phase 7 must focus on improving the existing implementation rather than introducing additional rendering features.

The primary objectives are:

1. Preserve the existing Reference fitting profile.
2. Establish reproducible quality and performance baselines.
3. Introduce an improved Quality fitting profile.
4. Improve convergence reliability for difficult BRDF configurations.
5. Reduce redundant numerical computation.
6. Improve CPU throughput and parallel scaling.
7. Detect and selectively refine poorly fitted LUT cells.
8. Evaluate improvements using independent numerical validation.
9. Provide systematic performance and quality benchmarks.
10. Preserve compatibility with existing CLI workflows and DDS output formats.

Primary reference:

https://github.com/selfshadow/ltc_code

Relevant reference files:

- `fit/fitLTC.cpp`
- `fit/nelder_mead.h`
- `fit/LTC.h`

**The fundamental requirement is that Reference Mode remains a stable compatibility baseline while Quality Mode is allowed to improve numerical methods and optimization behavior.**

Do not improve reported metrics by weakening validation, changing BRDF definitions, or silently modifying reference conventions.

---

## 1. Existing Project Assessment

Before implementing Phase 7, inspect the repository and document the existing implementation.

Review:

- BRDF evaluation and sampling.
- LTC distribution evaluation.
- MIS fitting error calculation.
- Nelder–Mead optimization.
- BRDF integration.
- Single-point fitting.
- Initial guess propagation.
- LUT generation scheduling.
- Checkpointing.
- DDS export.
- Numerical validation.
- Performance-sensitive code paths.

Identify existing optimizations before implementing new ones.

Do not duplicate working functionality.

### Initial Baseline

Run representative benchmarks using the current implementation.

Include:

- GGX.
- Beckmann.
- Disney Diffuse.

Evaluate multiple roughness and view-angle configurations.

Record:

- Fitting objective.
- Independent distribution error.
- Area-light integration error.
- Optimizer iterations.
- Objective evaluations.
- Convergence status.
- Time per cell.
- Total generation time.
- Memory usage.
- DDS quantization errors.

Where practical, use a fixed set of benchmark cases that can be rerun after each optimization.

Do not modify the algorithms before collecting the baseline.

---

## 2. Compatibility and Optimization Profiles

Maintain a strict separation between compatibility and improved-quality behavior.

### Reference Profile

The Reference profile must preserve the established implementation.

Its baseline includes:

- Reference-compatible parameterization.
- Deterministic midpoint sampling.
- Reference MIS error formulation.
- Cubic absolute error.
- Reference Nelder–Mead behavior.
- Existing convergence settings.
- Reference-compatible initial guesses.
- Existing BRDF integration conventions.

Do not introduce the following into Reference Mode:

- Adaptive sampling.
- Multi-start optimization.
- Alternative objective functions.
- Log-space parameter transformations.
- Automatic fitting retries.
- Modified convergence criteria.
- New numerical regularization.
- Automatic coefficient smoothing.

Performance optimizations may be shared with Reference Mode only if numerical compatibility is demonstrated.

If operation ordering or floating-point results change, keep the original execution path available.

### Quality Profile

The Quality profile may use:

- Higher sample counts.
- Adaptive sampling.
- Multiple initial guesses.
- Improved parameter constraints.
- Better convergence diagnostics.
- Candidate restarts.
- Independent candidate validation.
- Selective cell refinement.

All modifications must remain configurable.

### Suggested Configuration

```cpp
struct QualityFittingConfig
{
    uint32_t initialSampleResolution = 32;
    uint32_t maximumSampleResolution = 128;

    uint32_t maximumIterations = 300;
    uint32_t maximumRestarts = 4;

    bool adaptiveSampling = true;
    bool multiStart = true;
    bool logSpaceScales = true;

    double convergenceTolerance = 1e-7;
    double minimumScale = 1e-7;

    uint32_t validationSampleResolution = 128;
};
```

This is an illustrative configuration.

Choose actual defaults based on measurements rather than assumptions.

Do not require every optional optimization to be enabled.

---

## 3. Architecture

Extend the fitting system without changing its fundamental dependencies.

```text
                   LUT Generator
                         |
                         v
                    LTC Fitter
                         |
              +----------+----------+
              |                     |
              v                     v
       Reference Profile      Quality Profile
              |                     |
              |          +----------+----------+
              |          |          |          |
              |          v          v          v
              |      Adaptive   Multi-start  Constraints
              |      Sampling   Optimization
              |          |          |          |
              +----------+----------+----------+
                         |
                         v
                   Error Function
                         |
                  +------+------+
                  |             |
                  v             v
             BRDF Sampling   LTC Sampling
                         |
                         v
                  Fitting Result
                         |
               +---------+---------+
               |                   |
               v                   v
           LUT Storage        Validation
               |                   |
               v                   v
           DDS Export          Benchmarks
```

### Architectural Rules

- Keep the optimizer independent of BRDF implementations.
- Keep BRDF sampling independent of the LUT grid.
- Keep DDS export independent of fitting internals.
- Do not change the generic BRDF interface without demonstrated necessity.
- Keep Quality-specific algorithms isolated.
- Preserve the existing validation framework.
- Do not introduce GPU dependencies.

Avoid building an overly generic optimization framework that the project does not need.

---

## 4. Project Structure

Extend the existing implementation approximately as follows:

```text
include/ltc/
├── QualityFitting.h
├── AdaptiveSampling.h
├── InitialGuessStrategy.h
├── FittingDiagnostics.h
├── RefinementPolicy.h
└── Benchmark.h

src/
├── fitting/
│   ├── QualityFitter.cpp
│   ├── AdaptiveSampling.cpp
│   ├── MultiStartOptimizer.cpp
│   ├── InitialGuessStrategy.cpp
│   └── FittingDiagnostics.cpp
├── lut/
│   └── LUTRefinement.cpp
└── benchmark/
    ├── FittingBenchmark.cpp
    ├── GenerationBenchmark.cpp
    └── BenchmarkReport.cpp

tests/
├── AdaptiveSamplingTests.cpp
├── MultiStartTests.cpp
├── FittingStabilityTests.cpp
├── LUTRefinementTests.cpp
├── OptimizationRegressionTests.cpp
└── PerformanceRegressionTests.cpp

docs/
├── quality-fitting.md
├── convergence.md
├── cpu-performance.md
└── benchmarking.md
```

Adapt the layout to the actual repository.

Do not create unnecessary classes or placeholder files.

---

## 5. Baseline Benchmark Suite

Implement a reproducible benchmark suite before introducing numerical optimizations.

### BRDF Coverage

Include:

- GGX.
- Beckmann.
- Disney Diffuse.

Use representative parameter combinations.

For example:

| Roughness | NdotV | Purpose |
|---|---|---|
| 1.0 | 1.0 | Broad lobe, normal incidence |
| 0.7 | 0.8 | Moderate fitting |
| 0.5 | 0.5 | Typical oblique view |
| 0.2 | 0.3 | Sharp lobe |
| 0.05 | 0.1 | Difficult grazing configuration |

Use each BRDF's documented parameter mapping.

Do not treat every BRDF's roughness parameter as GGX alpha.

### Required Metrics

Record:

- Objective evaluations.
- Optimization iterations.
- Initial objective.
- Final objective.
- Independent validation objective.
- Convergence status.
- Total fitting duration.
- BRDF integration duration.
- Optimization duration.
- Approximation error.

### Benchmark Reproducibility

Record:

- Tool version.
- Compiler and version.
- Build configuration.
- CPU model when available.
- Thread count.
- BRDF implementation.
- Fitting profile.
- Configuration values.
- Sample counts.
- Numerical precision.

Keep runtime benchmarks separate from correctness tests.

Do not enforce fixed execution-time thresholds across different machines.

---

## 6. Adaptive Sampling

Implement adaptive sampling for Quality Mode.

The reference implementation uses a fixed deterministic sample grid.

Quality Mode should be able to increase sampling resolution when necessary.

### Initial Strategy

Use a configurable sequence such as:

```text
32 x 32
64 x 64
128 x 128
```

These are suggested levels.

The actual sequence must be configurable.

### Optimization Strategy

A recommended approach is staged optimization:

1. Fit using the initial sample resolution.
2. Evaluate the candidate using an independent validation sample set.
3. Increase the fitting sample resolution when needed.
4. Re-optimize using the previous result as the initial guess.
5. Stop when the configured validation criteria are satisfied or the computational budget is exhausted.

Avoid increasing sample resolution within a single objective evaluation.

The objective function should remain deterministic and fixed during each optimization stage.

### Important Numerical Constraint

Changing the sample resolution changes the numerical approximation of the objective function.

Therefore, do not directly compare raw objective values calculated with different sample grids as if they were identical measurements.

Use a common independent validation configuration for candidate comparisons.

### Sampling Requirements

- Deterministic execution.
- Stable sample ordering.
- Correct sampling PDFs.
- Consistent MIS weights.
- No hidden random state.
- Explicit sampling budgets.

Preserve the reference MIS formulation unless an alternative objective is explicitly selected.

### Adaptive Termination

Support termination based on:

- Validation error.
- Improvement between stages.
- Maximum sampling resolution.
- Maximum total objective evaluations.
- Maximum restart count.

Do not rely exclusively on optimizer convergence status.

---

## 7. Multi-start Optimization

Implement multiple initial guesses for difficult fitting configurations.

Nelder–Mead is a local derivative-free optimizer and may converge to different solutions depending on initialization.

Quality Mode should be able to explore multiple candidates.

### Candidate Initializations

Support:

1. Reference-compatible neighboring-cell initialization.
2. Identity LTC initialization.
3. Previously converged candidate.
4. Deterministic perturbations of a valid initial guess.

Do not introduce unrestricted random initialization.

If a pseudorandom strategy is needed later, it must use an explicit seed.

### Optimization Procedure

For each candidate:

1. Validate the initial matrix.
2. Run Nelder–Mead.
3. Record optimizer status.
4. Validate the resulting matrix.
5. Calculate the fitting objective.
6. Calculate an independent validation objective.
7. Record the candidate's numerical diagnostics.

Select the final candidate using a documented comparison rule.

### Candidate Selection

The default Quality Mode selection should prioritize:

1. Valid finite LTC matrices.
2. Valid magnitude and amplitude data.
3. Lower independent validation error.
4. Lower fitting objective when validation results are effectively equivalent.
5. Deterministic tie-breaking.

Do not automatically select a candidate solely because its optimizer reports `Converged`.

A converged result can still have a worse approximation error than a valid result that reached the iteration limit.

### Computational Budget

Provide explicit limits for:

- Maximum initial guesses.
- Maximum iterations per candidate.
- Maximum objective evaluations.
- Total fitting budget.

Multi-start fitting must not run indefinitely.

---

## 8. Improved Parameter Constraints

The reference optimizer clamps positive scale parameters to avoid invalid matrices.

Quality Mode may use a more stable parameterization.

### Log-Space Scale Parameters

For positive LTC scales:

\[
m_{11}=e^{s_1}
\]

\[
m_{22}=e^{s_2}
\]

where \(s_1,s_2\) are optimization parameters.

This guarantees positive scales for finite representable values, subject to numerical range checks.

The shear parameter may remain unconstrained within a documented valid range.

### Requirements

- Use log-space scaling only in Quality Mode.
- Apply finite bounds to prevent exponential overflow and underflow.
- Validate matrix invertibility.
- Reject invalid candidates.
- Preserve the reference matrix convention.
- Avoid modifying amplitude during shape optimization.

### Normal Incidence

For isotropic BRDFs at normal incidence:

\[
m_{11}=m_{22}
\]

\[
m_{13}=0
\]

Quality Mode may optimize the single independent scale parameter directly.

Do not modify the Reference Mode's normal-incidence optimization behavior.

### Conditioning

Record the matrix condition number or an equivalent stability metric.

Reject numerically singular matrices according to an explicit policy.

Do not reject a physically useful LTC merely because it is strongly anisotropic unless it violates a documented numerical stability requirement.

---

## 9. Convergence Diagnostics

Improve fitting diagnostics to distinguish optimization status from approximation quality.

### Required Diagnostics

Record:

- Initial objective.
- Final objective.
- Best independent validation error.
- Optimization iterations.
- Objective evaluations.
- Number of restarts.
- Final sample resolution.
- Minimum and maximum scale parameters.
- Matrix conditioning.
- Termination reason.
- Numerical failures.

### Termination Reasons

Support meaningful statuses such as:

```cpp
enum class QualityFitStatus
{
    Converged,
    ValidationTargetReached,
    MaximumIterations,
    MaximumEvaluations,
    MaximumSamplingResolution,
    Stagnated,
    InvalidMatrix,
    NumericalFailure
};
```

Adapt these to existing result types.

Do not convert all non-converged outcomes into a generic failure.

A valid best-effort result may remain useful.

### Stagnation Detection

Detect cases where:

- Objective improvement becomes negligible.
- Parameter updates become negligible.
- Repeated restarts produce equivalent solutions.
- Increasing sample resolution does not improve independent validation.

Use deterministic stopping rules.

Do not terminate prematurely based on a single small improvement.

---

## 10. CPU Performance Optimization

Profile the current implementation before modifying performance-sensitive code.

Identify actual bottlenecks.

Potential hotspots include:

- BRDF evaluation.
- BRDF importance sampling.
- LTC sampling.
- Matrix inversion.
- PDF evaluation.
- MIS error accumulation.
- Objective function construction.
- Memory allocations.
- Worker synchronization.

Optimize measured bottlenecks rather than making speculative changes.

### 10.1 BRDF Sample Precomputation

For a fixed BRDF context, many quantities remain unchanged across LTC optimization iterations.

Potentially reusable data includes:

- BRDF-sampled light directions.
- BRDF values at those directions.
- BRDF sampling PDFs.
- Sample-grid coordinates.
- BRDF integration results.

Implement a worker-local cache where appropriate.

### Important Constraint

BRDF-sampled data can be cached across objective evaluations only when it is independent of the candidate LTC matrix.

LTC-sampled directions depend on the candidate matrix and generally must be recomputed.

BRDF evaluation at those LTC-sampled directions must also be recomputed.

Do not cache candidate-dependent values incorrectly.

### Cache Correctness

Verify:

- Cached and uncached objectives agree within strict tolerance.
- Cache keys include all relevant BRDF parameters.
- Cache data is not reused between incompatible fitting contexts.
- No shared mutable cache races occur.
- Reference Mode behavior remains preserved.

Prefer per-fit or worker-local caches.

Do not introduce a global unbounded cache.

---

## 11. Objective Function Optimization

The MIS error evaluator is called repeatedly during optimization.

Reduce unnecessary work without changing its mathematical meaning.

Possible improvements:

- Reuse sample-grid coordinates.
- Avoid repeated invariant calculations.
- Minimize temporary allocations.
- Avoid virtual dispatch where profiling shows a meaningful cost and a safe optimization exists.
- Reuse working buffers.
- Avoid repeated conversion between mathematical representations.
- Optimize LTC matrix update calculations.

### Numerical Equivalence

For optimizations applied to Reference Mode, establish numerical equivalence against the original implementation.

For Quality Mode, validate that optimizations preserve the intended objective within documented numerical tolerance.

Do not silently change:

- MIS denominators.
- Sampling distributions.
- The cubic error exponent.
- BRDF normalization.
- LTC magnitude.
- The integration domain.

### Floating-Point Behavior

Be aware that:

- Reordering arithmetic can change rounding.
- Fused multiply-add can change results.
- Compiler fast-math settings can change numerical behavior.
- Different reduction orders can change accumulated errors.

Do not enable unsafe floating-point optimization flags globally.

If aggressive compiler optimizations are evaluated, isolate them and compare their numerical effects.

---

## 12. CPU Parallel Performance

Reuse the dependency-aware LUT scheduler established in Phase 3.

### Requirements

- Preserve fitting dependencies.
- Preserve deterministic initial guesses.
- Avoid duplicate fitting.
- Keep worker-local numerical state.
- Avoid excessive synchronization.
- Avoid long critical sections.
- Minimize checkpoint interference.
- Ensure clean shutdown on errors.

### Scheduler Optimization

Investigate:

- Worker utilization.
- Ready-job queue contention.
- Load imbalance between roughness rows.
- Time spent waiting for dependencies.
- Checkpoint serialization overhead.
- Progress reporting overhead.

Optimize only where profiling shows an actual problem.

### Determinism

Compare generation with:

```text
1 thread
2 threads
4 threads
8 threads
```

Under the same numerical configuration, worker count must not change the candidate initialization sequence or sample set.

Quality Mode must remain deterministic across scheduling choices.

### Independent Restarts

Multi-start candidates for one cell may be evaluated independently.

However, nested parallelism can cause CPU oversubscription.

Do not create additional worker pools inside every fitting job.

Use a clearly defined scheduling policy if candidate-level parallelism is implemented.

Prefer existing LUT-level parallelism unless benchmarks demonstrate a benefit from another strategy.

---

## 13. Selective LUT Refinement

Implement a Quality Mode refinement pass that identifies poorly fitted cells and selectively refits them.

The goal is to avoid spending the maximum fitting budget on every cell.

### Refinement Candidates

Identify cells based on:

- High independent distribution error.
- Poor convergence.
- Numerical instability.
- Invalid or nearly singular matrices.
- Large neighboring coefficient discontinuities.
- High interpolated-LUT error.
- High area-light error where diagnostic coverage exists.

Use configurable thresholds.

### Refinement Procedure

1. Generate an initial LUT.
2. Validate all cells or a documented deterministic subset.
3. Rank cells by quality metrics.
4. Select candidates for refinement.
5. Refit selected cells with increased sampling or multiple starts.
6. Validate both the original and refined candidates using the same independent validation configuration.
7. Accept a refined result only when it meets the configured improvement policy.
8. Preserve diagnostics and provenance.

### Important Dependency Constraint

The Phase 3 fitting traversal uses neighboring cells to initialize subsequent fits.

Refining one cell after the grid is generated does not automatically reproduce the original sequential fitting process for downstream cells.

Therefore:

- Treat post-generation refinement as a separate Quality Mode operation.
- Do not claim that refined coefficients reproduce reference traversal.
- Do not silently refit dependent cells.
- Document whether subsequent cells are left unchanged or explicitly regenerated.
- Preserve the original LUT for comparison.

### Refinement Acceptance

Do not replace a valid cell merely because the newly fitted optimizer reports convergence.

Compare independent validation errors and numerical stability.

Do not accept a refined cell that creates invalid interpolation or introduces severe local discontinuities.

Do not automatically smooth coefficients.

---

## 14. LUT Interpolation Quality

Use the Phase 5 validation framework to evaluate interpolation behavior.

A LUT may contain accurate individual cells but still produce errors between texel centers.

### Required Tests

Evaluate points:

- At texel centers.
- Between neighboring texels.
- Near roughness boundaries.
- Near grazing-angle boundaries.
- In regions of rapidly changing LTC coefficients.

Compare:

1. Independently fitted LTC at the requested parameters.
2. Full-precision LUT interpolation.
3. FP16 DDS interpolation.

### Metrics

Record:

- Distribution error.
- Matrix conditioning.
- Amplitude error.
- Area-light integration error.
- FP16 quantization contribution.

Keep interpolation error separate from fitting error.

### Constraints

Do not modify the LUT coordinate mapping in Reference Mode.

Do not change the Unreal export packing convention.

Do not introduce interpolation-specific coefficient transformations without explicit testing.

Adaptive LUT resolution or nonuniform grids are outside the Phase 7 scope.

---

## 15. Quality Validation

Use the independent validation system from Phase 5.

Quality improvements must be evaluated using data that is independent of the optimization objective samples.

### Required Comparisons

For each BRDF, compare:

- Reference Mode.
- Existing baseline Quality Mode, if present.
- Improved Quality Mode.

Use identical external validation scenarios.

### Distribution Metrics

Include:

- L1 error.
- L2 error.
- Cubic error.
- Integrated energy error.
- Worst-direction error.

### Lighting Metrics

Include:

- Mean absolute lighting error.
- Median relative lighting error.
- High-percentile relative error.
- Maximum absolute error.
- Worst lighting scenarios.
- Ground-truth uncertainty.

For high-percentile metrics, explicitly define the percentile used.

### Quality Assessment

An improvement in the fitting objective is useful but insufficient by itself.

Evaluate whether the new candidate also improves independent approximation or lighting accuracy.

Do not claim universal improvement based only on a handful of favorable examples.

Report regressions as well as improvements.

### BRDF Differences

Different BRDFs may benefit from different quality settings.

Permit BRDF-specific recommended presets without embedding BRDF-specific algorithms into the generic fitting engine.

For example, Beckmann may require different sampling budgets than Disney Diffuse.

Any presets must be documented and supported by measurements.

---

## 16. Benchmark Commands

Add a benchmark command to the existing CLI.

### Single-Point Benchmark

```bash
ltcgen benchmark fit \
    --brdf ggx \
    --alpha 0.25 \
    --ndotv 0.8 \
    --compare-profiles \
    --output ./reports/fit-benchmark.json
```

### LUT Generation Benchmark

```bash
ltcgen benchmark generate \
    --brdf ggx \
    --width 64 \
    --height 64 \
    --threads 8 \
    --profile quality \
    --output ./reports/ggx-benchmark.json
```

### Multi-BRDF Benchmark

```bash
ltcgen benchmark suite \
    --brdfs ggx,beckmann,disney-diffuse \
    --config ./benchmarks/quality.json \
    --output ./reports
```

### Worker Scaling Benchmark

```bash
ltcgen benchmark scaling \
    --brdf ggx \
    --threads 1,2,4,8 \
    --output ./reports/scaling.json
```

Adapt command syntax to existing CLI conventions.

### CLI Requirements

- Preserve existing commands.
- Use explicit profile selection.
- Support JSON reports.
- Support CSV export where useful.
- Record the full configuration.
- Report failed and skipped benchmark cases.
- Do not silently change the fitting profile.
- Do not benchmark incomplete or invalid LUT data as if it were valid.

---

## 17. Benchmark Reports

Generate reproducible reports.

### Required Fields

Include:

- Tool version.
- Build configuration.
- Compiler.
- CPU information when available.
- BRDF.
- Parameter mapping.
- Fitting profile.
- Sample counts.
- Optimizer configuration.
- Restart count.
- Thread count.
- Fitting duration.
- Objective evaluation count.
- Convergence status.
- Independent validation error.
- Area-light error metrics.
- Memory usage where available.

### Performance Comparison

Report improvements relative to the baseline.

For example:

```text
BRDF: GGX
Resolution: 64x64

Reference:
  Generation time: measured value
  Mean validation error: measured value

Quality:
  Generation time: measured value
  Mean validation error: measured value

Speed ratio: calculated value
Quality change: calculated value
```

Do not fabricate benchmark results.

### Performance-Quality Tradeoff

Report both quality and execution cost.

Higher numerical accuracy may require more processing time.

Do not describe a configuration as universally better if it substantially increases runtime without a corresponding meaningful quality improvement.

Include quality-versus-time comparisons where useful.

---

## 18. Checkpoint Compatibility

Phase 3 introduced resumable LUT generation.

Phase 7 must preserve this capability.

### Extended Metadata

Quality Mode checkpoints may require:

- Sampling stage.
- Restart configuration.
- Optimization budget.
- Quality profile version.
- Refinement state.
- Independent validation configuration.

If checkpoint formats change, increment the format version.

### Requirements

- Preserve existing Reference checkpoints where practical.
- Reject incompatible Quality checkpoints.
- Validate configuration identity.
- Preserve deterministic resume behavior.
- Do not silently resume with different optimization settings.
- Do not lose completed fitting diagnostics.

### Resume Behavior

A resumed Quality generation must produce results equivalent to an uninterrupted run under the same configuration.

If exact equality cannot be guaranteed, document the numerical tolerance and reason.

Test resumption across different worker counts.

---

## 19. Automated Tests

Extend GoogleTest and CTest.

### Adaptive Sampling Tests

Verify:

- Deterministic sample sequences.
- Correct sampling-stage transitions.
- Stable objective values within each stage.
- Correct maximum sample limits.
- Correct stopping conditions.
- Correct independent candidate comparisons.

### Multi-start Tests

Verify:

- Deterministic initial guesses.
- Correct restart limits.
- Valid candidate selection.
- Rejection of invalid candidates.
- Stable tie-breaking.
- Correct optimizer status reporting.

### Parameter Constraint Tests

Verify:

- Positive scale reconstruction.
- No exponential overflow.
- No numerical underflow producing invalid matrices.
- Correct normal-incidence constraints.
- Valid inverse matrices.
- Matrix condition checks.

### Cache Tests

Compare cached and uncached objective evaluations.

Test:

- GGX.
- Beckmann.
- Disney Diffuse.
- Different roughness values.
- Different view angles.
- Different sample resolutions.

Verify that cache invalidation is correct.

### Reference Regression Tests

Preserve fixed reference fitting cases.

Verify that:

- Reference configuration remains unchanged.
- Initial guesses remain unchanged.
- Objective evaluation remains compatible.
- Optimizer behavior remains compatible.
- DDS packing remains unchanged.

Use previously verified reference data.

Do not silently regenerate golden results.

### Quality Fitting Tests

Verify:

- Objective reduction.
- Independent validation.
- Matrix validity.
- Deterministic results.
- Explicit budget exhaustion.
- Correct failure reporting.

Do not require every difficult BRDF configuration to achieve an arbitrary universal numerical threshold.

### LUT Refinement Tests

Verify:

- Correct identification of poor cells.
- Correct candidate ranking.
- Correct refitting.
- Rejection of worse candidates.
- Preservation of original data.
- Correct checkpoint interaction.
- No accidental modification of Reference LUTs.

### Parallel Tests

Verify:

- Worker-count consistency.
- Thread safety.
- No duplicate jobs.
- No missing results.
- No deadlocks.
- Deterministic result ordering.

Run ThreadSanitizer where supported.

### Performance Tests

Maintain performance benchmarks separately from correctness tests.

Do not use fragile wall-clock assertions in normal CI.

---

## 20. Documentation

Update the following documents.

### docs/quality-fitting.md

Describe:

- Quality Mode architecture.
- Adaptive sampling.
- Multi-start optimization.
- Candidate selection.
- Numerical constraints.
- Validation-driven fitting.
- Differences from Reference Mode.

### docs/convergence.md

Describe:

- Nelder–Mead convergence behavior.
- Stagnation detection.
- Invalid candidate handling.
- Iteration and evaluation budgets.
- Restart behavior.
- Difficult BRDF regions.

### docs/cpu-performance.md

Describe:

- Performance bottlenecks.
- Sample caching.
- Memory allocation strategy.
- Worker scheduling.
- Determinism.
- Numerical effects of optimizations.
- Benchmark findings.

### docs/benchmarking.md

Describe:

- Benchmark commands.
- Benchmark configurations.
- Reference environments.
- Quality metrics.
- Performance metrics.
- Report schema.
- How to reproduce measurements.

Update README.md with Quality Mode examples.

Clearly explain the difference between Reference and Quality profiles.

---

## 21. Scope Restrictions

Phase 7 must not implement:

- New BRDF models.
- Anisotropic BRDF fitting.
- Multiple LTC lobes.
- New GPU rendering features.
- GPU fitting.
- CUDA.
- DirectCompute.
- Vulkan Compute.
- Unreal Engine plugins.
- Unreal Engine shader modifications.
- GUI.
- Distributed fitting.
- Dynamic BRDF plugins.
- Automatic machine-learning-based fitting.
- Arbitrary new optimization algorithms.
- Nonuniform LUT grids.
- Runtime LUT generation.
- Path-tracing features.

Do not expand Phase 7 into a complete numerical optimization research framework.

Focus on practical improvements to the existing CPU-based LTC generator.

---

## 22. Acceptance Criteria

Phase 7 is complete when:

- [ ] A reproducible baseline benchmark suite exists.
- [ ] Existing Reference Mode behavior is preserved.
- [ ] Quality Mode has explicit configuration.
- [ ] Adaptive sampling is implemented and tested.
- [ ] Sampling stages preserve deterministic objectives.
- [ ] Multi-start optimization is implemented.
- [ ] Candidate selection uses independent validation.
- [ ] Numerical parameter constraints are improved.
- [ ] Normal-incidence fitting remains mathematically consistent.
- [ ] Convergence diagnostics are extended.
- [ ] Invalid numerical candidates are handled safely.
- [ ] CPU performance bottlenecks are measured.
- [ ] Redundant BRDF evaluation is reduced where practical.
- [ ] Sample caching is verified.
- [ ] Objective function optimizations preserve mathematical correctness.
- [ ] Parallel generation remains deterministic.
- [ ] Selective LUT refinement is implemented.
- [ ] Refinement does not silently change Reference Mode.
- [ ] LUT interpolation quality is evaluated.
- [ ] Independent validation metrics are reported.
- [ ] Area-light accuracy is compared against the baseline.
- [ ] GGX, Beckmann, and Disney Diffuse are benchmarked.
- [ ] Performance-quality tradeoffs are documented.
- [ ] Benchmark CLI commands work.
- [ ] JSON benchmark reports are generated.
- [ ] Checkpoint and resume remain compatible.
- [ ] Existing DDS export formats remain unchanged.
- [ ] All Phase 0–6 regression tests pass.
- [ ] The project remains CPU-only and C++20-based.
- [ ] Documentation is updated.

Performance improvements must be demonstrated using actual measurements.

Quality improvements must be supported by independent numerical validation.

Do not claim improvement when the evidence shows only equivalent or mixed results.

---

## 23. Implementation Procedure

Implement Phase 7 incrementally.

### Stage A — Baseline and Profiling

1. Inspect the existing repository.
2. Review the current Reference and Quality profiles.
3. Define reproducible benchmark configurations.
4. Run baseline fitting benchmarks.
5. Run baseline area-light validation.
6. Profile CPU execution.
7. Identify the main numerical and performance bottlenecks.
8. Preserve benchmark data for later comparisons.

### Stage B — Fitting Quality

9. Implement staged adaptive sampling.
10. Add deterministic sampling tests.
11. Implement multi-start optimization.
12. Add candidate validation and selection.
13. Implement optional log-space scale optimization.
14. Extend convergence diagnostics.
15. Verify difficult low-roughness and grazing cases.
16. Compare against the original fitting baseline.

### Stage C — Performance Optimization

17. Implement safe BRDF sample precomputation.
18. Add cache-correctness tests.
19. Reduce unnecessary allocations.
20. Profile matrix and PDF evaluation.
21. Optimize measured hotspots.
22. Evaluate worker scheduling efficiency.
23. Benchmark sequential and parallel performance.
24. Verify numerical compatibility after each optimization.

### Stage D — LUT Refinement

25. Implement per-cell quality ranking.
26. Implement selective Quality Mode refitting.
27. Add refinement acceptance tests.
28. Validate neighboring coefficient continuity.
29. Evaluate interpolated LUT quality.
30. Verify checkpoint and resume behavior.

### Stage E — Validation and Documentation

31. Run full GGX quality comparisons.
32. Run Beckmann quality comparisons.
33. Run Disney Diffuse quality comparisons.
34. Evaluate representative area-light scenarios.
35. Compare fitting quality and CPU performance.
36. Identify and document regressions.
37. Run the complete test suite.
38. Update documentation.
39. Produce the final Phase 7 report.

Do not optimize everything simultaneously.

Make changes incrementally so numerical regressions can be attributed to specific modifications.

---

## 24. Final Implementation Report

After implementation, provide:

1. Summary of implemented improvements.
2. Files created and modified.
3. Baseline benchmark methodology.
4. Reference Mode compatibility results.
5. Quality Mode configuration and algorithms.
6. Adaptive sampling behavior.
7. Multi-start optimization results.
8. Numerical stability improvements.
9. Convergence statistics.
10. Sample caching and objective optimization results.
11. CPU profiling findings.
12. Single-thread and multithread performance.
13. LUT refinement results.
14. Interpolation quality measurements.
15. GGX numerical validation results.
16. Beckmann numerical validation results.
17. Disney Diffuse numerical validation results.
18. Area-light approximation error comparisons.
19. Performance-quality tradeoffs.
20. Checkpoint and resume compatibility.
21. Build and test results.
22. Known limitations and regressions.
23. Deviations from this specification and their rationale.
24. Recommendations for subsequent development.

**Expected result: A stable, deterministic, CPU-based LTC fitting system with an improved Quality Mode, measurable convergence and performance improvements, validated multi-BRDF support, and unchanged Reference Mode and DDS compatibility.**
