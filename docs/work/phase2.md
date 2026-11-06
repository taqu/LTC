# Phase 2 — LTC Numerical Fitting Engine

## Project: LTC LUT Generator

### Objective

Implement a reusable, CPU-based numerical fitting engine that approximates a BRDF using a single Linearly Transformed Cosine (LTC) distribution.

The fitting engine must use the mathematical core established in Phase 1 and provide:

- BRDF energy (magnitude) integration.
- Average BRDF direction estimation.
- Schlick Fresnel coefficient integration.
- Multiple Importance Sampling (MIS) error evaluation.
- Nelder–Mead numerical optimization.
- Stable LTC parameter constraints.
- Single-point LTC fitting.
- Numerical validation and reference comparisons.
- Deterministic, reproducible results.

The primary reference implementation is:

https://github.com/selfshadow/ltc_code

Important reference files:

- `fit/fitLTC.cpp`
- `fit/nelder_mead.h`
- `fit/LTC.h`
- `fit/brdf.h`
- `fit/brdf_ggx.h`

The long-term objective is to generate DDS textures compatible with real-time area-light LTC evaluation, including Unreal Engine's GGX LTC conventions.

**Phase 2 must implement the numerical fitting engine only. Do not implement LUT grid generation or DDS writing.**

---

## 1. Existing Project Integration

Phase 0 established the CLI, DDS inspection functionality, and automated testing infrastructure.

Phase 1 established:

- Reusable mathematical utilities.
- A generic BRDF interface.
- Isotropic GGX BRDF evaluation.
- GGX sampling and PDF calculation.
- LTC distribution evaluation.
- LTC matrix construction.
- LTC sampling and PDF evaluation.
- Numerical validation tests.

Before modifying the project:

1. Inspect the existing source code and public interfaces.
2. Verify the mathematical conventions implemented in Phase 1.
3. Review the authoritative LTC fitting reference.
4. Identify any incompatibilities between the existing implementation and the reference.
5. Preserve all working Phase 0 and Phase 1 functionality.

Do not rewrite the existing mathematical core unless a correctness issue is identified.

If an issue is discovered, fix it with a regression test and document the reason.

### Important Architectural Rule

The fitting engine must depend on the generic BRDF interface, not on the concrete GGX implementation.

GGX is the initial reference BRDF, but future Beckmann, Disney Diffuse, and custom BRDF implementations must be usable without changing the optimizer.

---

## 2. Project Structure

Extend the existing project approximately as follows:

```text
include/ltc/
├── Fitter.h
├── FittingConfig.h
├── FittingResult.h
├── BRDFIntegration.h
├── ErrorFunction.h
└── NelderMead.h

src/
├── fitting/
│   ├── LTCFitter.cpp
│   ├── BRDFIntegration.cpp
│   ├── ErrorFunction.cpp
│   ├── NelderMead.cpp
│   └── FittingDiagnostics.cpp
└── cli/
    └── main.cpp

tests/
├── BRDFIntegrationTests.cpp
├── ErrorFunctionTests.cpp
├── NelderMeadTests.cpp
├── LTCFitterTests.cpp
└── FittingReferenceTests.cpp

docs/
├── fitting.md
├── optimization.md
└── fitting-validation.md
```

Adapt this structure to established repository conventions.

Do not create empty placeholder files.

The numerical optimizer must not depend on BRDF or LTC-specific data structures.

---

## 3. Fitting Configuration

Introduce an explicit configuration object.

Suggested interface:

```cpp
enum class FittingProfile
{
    Reference,
    Quality
};

struct FittingConfig
{
    FittingProfile profile = FittingProfile::Reference;

    uint32_t sampleResolution = 32;
    uint32_t maxIterations = 100;

    double simplexStep = 0.05;
    double tolerance = 1e-5;

    double minScale = 1e-7;

    bool useMIS = true;
};
```

This interface is illustrative and may be adjusted.

### Reference Profile

The Reference profile must reproduce the original LTC fitting algorithm as closely as practical.

Use:

| Parameter | Value |
|---|---|
| BRDF integration samples | 32 × 32 |
| Error evaluation samples | 32 × 32 per sampling strategy |
| Sampling grid | Deterministic midpoint grid |
| Error exponent | 3 |
| Optimization | Nelder–Mead |
| Initial simplex step | 0.05 |
| Convergence tolerance | 1e-5 |
| Maximum iterations | 100 |
| Minimum positive scale | 1e-7 |

The minimum positive scale is a fitting-parameter constraint, not the GGX roughness floor.

Do not confuse it with `MIN_ALPHA = 1e-5` used by the reference LUT parameterization.

Read the reference optimizer source carefully to reproduce its simplex initialization, termination criteria, and update rules.

### Quality Profile

Provide an extensible configuration for higher-quality fitting.

Initially, it may use the same algorithms with configurable sample counts and iteration limits.

Do not introduce a different optimization algorithm in this phase.

Any changes to the reference algorithm must remain isolated from Reference mode.

### Precision Policy

The Phase 1 mathematical core uses double precision by default.

Maintain double precision for the default fitting implementation.

However, recognize that the original reference implementation uses float arithmetic.

Do not claim bitwise compatibility with the reference when executing double-precision calculations.

Document precision differences explicitly.

If reference-compatible float execution is already supported by the Phase 1 architecture, preserve it.

Do not substantially redesign the math layer solely to add float compatibility in Phase 2.

---

## 4. Deterministic Sampling

The reference implementation uses a regular midpoint grid in the unit square.

For sample indices `i` and `j`:

\[
u_1=\frac{i+0.5}{N}
\]

\[
u_2=\frac{j+0.5}{N}
\]

where `N` is the sample resolution.

Implement a reusable deterministic sampling utility.

Suggested interface:

```cpp
struct Sample2D
{
    double u1;
    double u2;
};

class SampleGrid
{
public:
    explicit SampleGrid(uint32_t resolution);

    uint32_t resolution() const;

    Sample2D sample(
        uint32_t x,
        uint32_t y) const;
};
```

### Requirements

- No global random number generator.
- No nondeterministic sampling.
- Stable sample order.
- Validate resolution greater than zero.
- Avoid integer overflow when calculating total sample count.
- Reuse the same sample grid for reference-compatible BRDF and LTC sampling.

The fitter must produce identical results when executed multiple times with identical inputs under the same build and floating-point environment.

Do not introduce multithreading yet.

---

## 5. BRDF Integration

Implement BRDF integration independently of the optimizer.

The integration module must calculate:

1. BRDF magnitude.
2. Schlick Fresnel coefficient.
3. Average BRDF direction.

### 5.1 Magnitude

The magnitude is the hemispherical integral of the cosine-weighted BRDF:

\[
A(V,\alpha)=
\int_{\Omega^+}
f_{\mathrm{target}}(V,L,\alpha)
\,d\omega_L
\]

Estimate it using BRDF importance sampling:

\[
A\approx
\frac{1}{N^2}
\sum_{i=1}^{N^2}
\frac{f_{\mathrm{target}}(V,L_i,\alpha)}
{p_{\mathrm{BRDF}}(L_i)}
\]

Only include samples with valid positive PDF values.

The denominator must remain the total number of generated samples, matching the reference integration method.

Do not divide by the number of valid samples.

Otherwise, the estimate becomes biased.

### 5.2 Schlick Fresnel Coefficient

Calculate:

\[
F_c\approx
\frac{1}{N^2}
\sum_i
\frac{f_{\mathrm{target}}(V,L_i,\alpha)}
{p_{\mathrm{BRDF}}(L_i)}
(1-V\cdot H_i)^5
\]

where:

\[
H_i=\frac{V+L_i}{\|V+L_i\|}
\]

The coefficient is used later for Schlick Fresnel reconstruction.

For a suitable Fresnel-free GGX target:

\[
A(F_0)=F_0(A-F_c)+F_c
\]

The fitting engine must not hardcode material-specific `F0`.

### Generic BRDF Requirement

Do not assume every future BRDF supports the same Fresnel decomposition.

Expose the Fresnel coefficient as an optional or explicitly supported integration result.

For a BRDF without a compatible half-vector Fresnel model, magnitude and direction integration must still work.

### 5.3 Average Direction

Calculate the weighted first moment:

\[
\mathbf d=
\sum_i
\frac{f_{\mathrm{target}}(V,L_i,\alpha)}
{p_{\mathrm{BRDF}}(L_i)}
L_i
\]

Normalize it:

\[
\overline L=
\frac{\mathbf d}{\|\mathbf d\|}
\]

For isotropic BRDFs, the reference implementation removes the Y component before normalization.

Preserve this behavior in Reference mode.

The average direction is used to orient the LTC distribution before optimizing its shape parameters.

### Degenerate Cases

Handle:

- Zero magnitude.
- Near-zero accumulated direction.
- Invalid BRDF samples.
- Zero or non-finite PDF.
- Non-finite BRDF evaluations.
- Invalid half-vectors.

If magnitude is effectively zero, return an explicit zero-energy result rather than attempting to normalize a zero vector.

Distinguish valid zero contributions from violations of the BRDF contract.

### Suggested API

```cpp
struct BRDFIntegrationResult
{
    double magnitude = 0.0;

    double fresnelCoefficient = 0.0;
    bool hasFresnelCoefficient = false;

    Vec3 averageDirection{0.0, 0.0, 1.0};

    bool hasValidDirection = false;
};

class BRDFIntegrator
{
public:
    BRDFIntegrationResult integrate(
        const IBRDF& brdf,
        const BRDFContext& context,
        const FittingConfig& config) const;
};
```

Do not introduce LUT-specific types into this interface.

---

## 6. MIS-Based Fitting Error

Implement a reusable error function that compares a target BRDF against a candidate LTC distribution.

The reference algorithm evaluates both distributions using Multiple Importance Sampling.

### 6.1 Target Functions

Let:

\[
f_B(L)=f_{\mathrm{target}}(V,L,\alpha)
\]

and:

\[
f_L(L)=A\,D_{\mathrm{LTC}}(L;M)
\]

where:

- \(A\) is the precomputed magnitude.
- \(M\) is the candidate LTC transformation.
- \(D_{\mathrm{LTC}}\) is a normalized PDF.

The fitting operation optimizes the LTC shape, not the magnitude.

### 6.2 Error Function

Use the cubic absolute difference:

\[
g(L)=|f_B(L)-f_L(L)|^3
\]

The reference error estimate uses samples drawn from both distributions:

\[
E(M)\approx
\frac{1}{N^2}
\sum_{i=1}^{N^2}
\left[
\frac{g(L_i^B)}
{p_B(L_i^B)+p_L(L_i^B)}
+
\frac{g(L_i^L)}
{p_B(L_i^L)+p_L(L_i^L)}
\right]
\]

where:

- \(L_i^B\) is sampled from the BRDF.
- \(L_i^L\) is sampled from LTC.
- \(p_B\) is the BRDF sampling PDF.
- \(p_L\) is the normalized LTC PDF.

This corresponds to the reference's balance-heuristic mixture estimator with equal sample counts.

Do not add an extra factor of one-half.

Do not divide by `2 * N * N`.

Do not replace the denominator with only the PDF of the sampling strategy that generated the direction.

### 6.3 Implementation Requirements

For each midpoint sample:

1. Sample a direction from the LTC distribution.
2. Evaluate BRDF value and BRDF PDF.
3. Evaluate amplitude-scaled LTC value and normalized LTC PDF.
4. Calculate the cubic difference.
5. Accumulate the MIS-weighted error.
6. Sample a direction from the BRDF.
7. Evaluate both distributions at that direction.
8. Accumulate the second contribution.

Repeat for all samples.

Use double precision for error accumulation.

Do not calculate the LTC PDF by dividing its evaluation by magnitude when a direct normalized `pdf()` method exists.

Use:

```cpp
double ltcValue = ltc.evaluate(direction);
double ltcPdf   = ltc.pdf(direction);
```

This avoids division-by-zero problems when magnitude is zero.

### PDF Support Requirements

The target BRDF may be zero outside the surface hemisphere, while the transformed LTC may have support below the surface.

Evaluate the distributions consistently over their full relevant support.

Do not discard LTC samples merely because their directions lie below the surface.

The BRDF evaluator should return the correct zero contribution and corresponding sampling PDF.

If both PDFs are zero at a queried direction, the contribution is zero only when the associated error contribution is also zero.

Otherwise, treat the situation as an invalid support or numerical condition.

Never silently divide by zero.

### Reference Compatibility

The reference uses:

- Cubic absolute error.
- Two sampling strategies.
- Equal numbers of samples.
- Deterministic midpoint sampling.
- Division by the sum of the two PDFs.

Preserve these characteristics in Reference mode.

### Suggested Interface

```cpp
class LTCErrorFunction
{
public:
    double evaluate(
        const IBRDF& brdf,
        const BRDFContext& context,
        const LTC& ltc,
        const FittingConfig& config) const;
};
```

The error evaluator must be independently testable.

It must not start or control the optimizer.

---

## 7. Nelder–Mead Optimizer

Implement a reusable Nelder–Mead optimizer.

The optimizer must not depend on LTC, GGX, or any particular BRDF.

### Required Operations

Implement:

- Initial simplex construction.
- Objective evaluation.
- Reflection.
- Expansion.
- Contraction.
- Shrinking.
- Convergence detection.
- Maximum iteration termination.
- Best candidate selection.

Use the reference optimizer as the compatibility baseline.

### Reference Settings

Use:

```text
Initial simplex step: 0.05
Convergence tolerance: 1e-5
Maximum iterations: 100
```

These values belong to the Reference profile.

### Parameter Dimensions

Support both:

- One-dimensional optimization.
- Three-dimensional optimization.

The original reference implementation uses a three-dimensional optimizer even for normal-incidence isotropic fits, where some parameters are ignored.

For strict reference compatibility, reproduce that behavior if necessary.

For the independent Quality profile, permit a reduced one-dimensional optimization at normal incidence to avoid redundant degrees of freedom.

Do not allow this difference to silently alter Reference mode.

### Suggested API

```cpp
enum class OptimizationStatus
{
    Converged,
    MaxIterations,
    InvalidInitialPoint,
    InvalidObjective,
    NumericalFailure
};

struct OptimizationResult
{
    std::vector<double> parameters;

    double objectiveValue = 0.0;

    uint32_t iterations = 0;
    uint64_t evaluations = 0;

    OptimizationStatus status;
};

class NelderMead
{
public:
    using Objective =
        std::function<double(
            std::span<const double>)>;

    OptimizationResult minimize(
        const Objective& objective,
        std::span<const double> initial,
        double initialStep,
        double tolerance,
        uint32_t maxIterations) const;
};
```

A fixed-size or templated implementation is also acceptable.

Avoid heap allocations inside the optimization loop where practical.

### Numerical Robustness

The optimizer must:

- Reject non-finite initial parameters.
- Detect non-finite objective values.
- Handle constant objective functions.
- Handle degenerate simplexes.
- Avoid infinite loops.
- Return the best valid candidate found.
- Preserve diagnostic information on failure.

### Testing

Test Nelder–Mead independently using analytical functions:

1. One-dimensional quadratic.
2. Three-dimensional quadratic.
3. Rosenbrock function.
4. Constant function.
5. Objective returning non-finite values.
6. Maximum iteration termination.

Verify:

- Approximate optimum.
- Objective reduction.
- Correct stopping condition.
- Deterministic execution.
- Reasonable evaluation count.

Do not use LTC fitting as the only test of the optimizer.

---

## 8. LTC Parameter Optimization

Implement the LTC-specific adapter that maps optimizer parameters to an LTC transformation.

The fitting parameters are:

\[
\mathbf p=(m_{11},m_{22},m_{13})
\]

For valid parameters:

\[
m_{11}>0,\quad m_{22}>0
\]

The transformation matrix and its inverse must remain valid.

### 8.1 General View Direction

For non-normal incidence, optimize:

- `m11`
- `m22`
- `m13`

Construct the LTC basis using the average BRDF direction.

The reference uses a basis conceptually equivalent to:

```cpp
Vec3 Z = averageDirection;
Vec3 X = Vec3(Z.z, 0.0, -Z.x);
Vec3 Y = Vec3(0.0, 1.0, 0.0);
```

For an isotropic BRDF whose average direction lies in the XZ plane, these form an orthonormal basis when `Z` is normalized.

Validate the basis before using it.

Do not apply arbitrary Gram–Schmidt adjustments in Reference mode unless necessary to fix a demonstrated numerical issue.

### 8.2 Normal Incidence

At normal incidence, an isotropic BRDF is rotationally symmetric.

Constrain the LTC to:

\[
m_{11}=m_{22}
\]

\[
m_{13}=0
\]

Use the identity basis.

Reference mode should preserve the reference optimizer's handling of this constraint.

Quality mode may directly optimize the single independent scale.

### 8.3 Parameter Constraints

The reference clamps positive scale parameters to a small minimum before constructing the LTC matrix.

Preserve this behavior in Reference mode.

Do not introduce a logarithmic parameter transformation in Reference mode.

Log-space optimization may be considered in later quality improvements.

Reject or penalize matrices that cannot be evaluated safely.

When the objective encounters an invalid candidate, return a documented finite penalty or use a clearly defined invalid-candidate strategy.

Avoid propagating NaN through the optimizer.

### 8.4 Initial Parameters

Support an explicit initial guess.

Suggested structure:

```cpp
struct LTCInitialGuess
{
    double m11 = 1.0;
    double m22 = 1.0;
    double m13 = 0.0;
};
```

The default initial guess is the identity transformation.

Phase 3 will use neighboring fitted cells as initial guesses.

Do not implement neighboring-cell propagation in Phase 2.

### 8.5 Optimization Behavior

The LTC fitter should:

1. Validate input parameters.
2. Calculate BRDF integrated terms.
3. Determine the LTC orientation.
4. Initialize LTC shape parameters.
5. Construct the error objective.
6. Run Nelder–Mead.
7. Construct the final LTC.
8. Evaluate the final fitting error.
9. Return coefficients and diagnostics.

The fitting engine must not modify the supplied BRDF.

Avoid retaining references to temporary contexts beyond the fitting call.

---

## 9. Fitting Result

Expose a structured result suitable for future LUT generation.

Suggested representation:

```cpp
enum class FitStatus
{
    Converged,
    MaxIterations,
    ZeroEnergy,
    InvalidInput,
    NumericalFailure
};

struct LTCParameters
{
    double m11 = 1.0;
    double m22 = 1.0;
    double m13 = 0.0;

    Mat3 basis{};
};

struct FittingResult
{
    LTCParameters parameters;

    double magnitude = 0.0;
    double fresnelCoefficient = 0.0;

    bool hasFresnelCoefficient = false;

    Vec3 averageDirection{};

    double initialError = 0.0;
    double finalError = 0.0;

    uint32_t iterations = 0;
    uint64_t evaluations = 0;

    FitStatus status;
};
```

Include the full LTC matrix and inverse matrix through either the result or a documented conversion method.

### Requirements

- No DDS-specific fields.
- No LUT row or column indices.
- No dependency on output image formats.
- Preserve optimizer status.
- Preserve numerical diagnostics.
- Preserve magnitude and Fresnel coefficients independently.
- Do not report convergence when the optimizer merely reached its iteration limit.

The result must contain enough information for Phase 3 to store a fitted LTC cell without rerunning the optimization.

---

## 10. Single-Point Fitting API

Expose a high-level fitting API.

Example:

```cpp
class LTCFitter
{
public:
    FittingResult fit(
        const IBRDF& brdf,
        const BRDFContext& context,
        const FittingConfig& config,
        const LTCInitialGuess& initial = {}) const;
};
```

### Example Usage

```cpp
GGXBRDF ggx;

BRDFContext context{
    .view = normalize(Vec3{0.6, 0.0, 0.8}),
    .alpha = 0.25
};

FittingConfig config;
config.profile = FittingProfile::Reference;

LTCFitter fitter;

FittingResult result =
    fitter.fit(ggx, context, config);
```

This example is illustrative. Adapt names to the actual project APIs.

### Determinism

Repeated calls with the same arguments must produce consistent results.

Do not use:

- Hidden random state.
- Global mutable optimizer state.
- Nondeterministic task scheduling.
- Parallel reductions.

Make all fitting-specific state local to the fitting operation.

---

## 11. CLI Integration

Add a command for testing single-point fitting.

For example:

```bash
ltcgen fit \
    --brdf ggx \
    --alpha 0.25 \
    --ndotv 0.8 \
    --profile reference
```

This command must fit only one LTC distribution.

Do not generate an entire LUT.

### Parameters

Support:

- `--brdf`
- `--alpha`
- `--ndotv`
- `--profile`
- `--samples`
- `--max-iterations`
- `--json`

Use `alpha` directly.

Do not silently interpret it as perceptual roughness.

Validate:

\[
0<\alpha\leq1
\]

\[
0<N\cdot V\leq1
\]

For the reference profile, document that the full LUT generator will later apply its own roughness and view-angle parameterization, including the grazing-angle cap.

### Example Output

```text
BRDF: GGX
Alpha: 0.25
NdotV: 0.8
Profile: Reference

LTC Parameters:
  m11: ...
  m22: ...
  m13: ...

Magnitude: ...
Fresnel Coefficient: ...

Initial Error: ...
Final Error: ...

Iterations: ...
Objective Evaluations: ...
Status: Converged
```

Actual values must come from the fitting process.

Do not use fabricated numerical values.

### JSON Output

Support:

```bash
ltcgen fit \
    --brdf ggx \
    --alpha 0.25 \
    --ndotv 0.8 \
    --json
```

The JSON must contain:

- Input parameters.
- Fitting configuration.
- LTC coefficients.
- Magnitude.
- Fresnel coefficient, when supported.
- Average direction.
- Initial error.
- Final error.
- Optimizer status.
- Iteration count.
- Objective evaluation count.

JSON output must not contain invalid NaN or infinity literals.

Handle non-finite diagnostic values using documented null values or explicit error fields.

### CLI Compatibility

Preserve existing commands:

- `--help`
- `--version`
- `inspect`

Do not break existing DDS inspection behavior.

---

## 12. Numerical Validation

Testing must verify the optimizer, integration routines, and final fitted LTC independently.

### 12.1 BRDF Integration Tests

Test:

- Magnitude is finite and nonnegative.
- Fresnel coefficient is finite and nonnegative when supported.
- Average direction is normalized when valid.
- Normal-incidence symmetry.
- Grazing-angle behavior.
- Low roughness.
- High roughness.
- Deterministic integration.

Use independently calculated analytical or high-sample-count numerical reference values.

For the Schlick Fresnel basis term, verify the expected physical range when the target BRDF is nonnegative and the half-vector is valid.

### 12.2 MIS Error Tests

Test:

- Identical target and approximation produce zero error, within numerical tolerance.
- Positive errors for deliberately different distributions.
- Nonnegative objective values.
- Finite values for valid parameters.
- Correct handling of zero-magnitude LTC.
- Correct handling of zero PDFs.
- Correct contribution from both sampling strategies.
- Deterministic repeated evaluations.

Create a synthetic BRDF adapter that exactly represents a known LTC distribution.

This allows an exact or near-exact fitting-error test independent of GGX.

Be careful to provide a sampling PDF consistent with the synthetic BRDF's sampling strategy.

### 12.3 Optimizer Tests

Verify convergence using analytical functions.

Do not rely exclusively on real BRDF fitting cases.

### 12.4 GGX Fitting Tests

Test representative combinations of roughness and view angle.

At minimum:

| Alpha | NdotV | Purpose |
|---|---|---|
| 1.0 | 1.0 | High roughness, normal incidence |
| 0.5 | 1.0 | Normal-incidence symmetry |
| 0.25 | 0.8 | Typical fitting case |
| 0.1 | 0.5 | Narrower specular lobe |
| 0.05 | 0.2 | Challenging grazing case |

For each case:

- Verify that the fitting result is finite.
- Verify that the matrix is invertible.
- Verify that magnitude is valid.
- Verify deterministic output.
- Verify that optimization reduces or preserves the objective.
- Record convergence status.
- Record iteration and evaluation counts.

Do not require every difficult case to converge within 100 iterations without first establishing a reference baseline.

A finite best-effort result with explicit `MaxIterations` status is preferable to falsely reporting convergence.

### 12.5 Reference Comparison

Compare selected single-point results against the original LTC reference implementation.

Important comparison targets:

- Integrated magnitude.
- Schlick Fresnel coefficient.
- Average BRDF direction.
- Initial fitting error.
- Final fitting error.
- Fitted LTC parameters.
- Optimizer iteration count.

Use the same input parameters and configuration.

Be aware that the reference uses float arithmetic and specific optimizer behavior.

Numerical differences are acceptable when explained, but mathematical inconsistencies are not.

Do not invent expected reference values.

Where possible, build a small independent reference test harness from the authoritative source.

Respect the reference implementation's license.

### 12.6 Independent Error Validation

Do not validate the fitted result using only the same deterministic samples used during fitting.

Add an independent higher-resolution numerical evaluation mode for tests.

For example:

- Fitting: 32 × 32 samples.
- Validation: 128 × 128 samples.

This helps detect solutions that fit the optimization sample grid but generalize poorly.

Keep validation metrics separate from the optimization objective.

The independent evaluation may use the same mathematical error definition while using a different deterministic sample set or integration resolution.

---

## 13. Performance and Diagnostics

Phase 2 is CPU-only.

Performance optimization is secondary to correctness.

However, provide useful fitting diagnostics:

- BRDF integration duration.
- Optimization duration.
- Objective evaluation count.
- Number of iterations.
- Initial error.
- Final error.
- Convergence status.

Use `std::chrono` for timing.

Do not include timing in deterministic reference comparisons.

### Performance Requirements

- Avoid unnecessary heap allocations inside inner sampling loops.
- Avoid repeated construction of invariant BRDF contexts.
- Keep matrix updates reasonably efficient.
- Avoid logging inside the objective evaluation loop.
- Avoid introducing GPU dependencies.
- Do not introduce multithreading in this phase.

If BRDF samples can be precomputed without changing the reference numerical behavior, this may be considered as a local optimization.

Document any such optimization and verify that it does not change fitting results beyond the established tolerance.

---

## 14. Documentation

Update the project documentation.

### docs/fitting.md

Document:

- The fitting pipeline.
- BRDF magnitude integration.
- Schlick Fresnel coefficient.
- Average direction estimation.
- LTC shape parameters.
- MIS error function.
- Normal-incidence constraints.
- Single-point fitting API.

### docs/optimization.md

Document:

- Nelder–Mead algorithm.
- Simplex initialization.
- Reflection, expansion, contraction, and shrink steps.
- Convergence conditions.
- Parameter constraints.
- Failure handling.
- Reference and Quality profiles.

### docs/fitting-validation.md

Document:

- Test cases.
- Analytical optimizer tests.
- Reference comparisons.
- Numerical tolerances.
- Independent error validation.
- Known difficult fitting regions.

### README.md

Update the feature list and CLI examples.

Clearly distinguish implemented functionality from later phases.

Do not claim full LUT generation is available.

---

## 15. Scope Restrictions

Phase 2 must not implement:

- LUT grid generation.
- Roughness/view-angle grid traversal.
- Neighboring-cell initialization.
- Multithreaded fitting.
- DDS writing.
- DDS LUT packing.
- Unreal Engine-specific texture packing.
- Spherical-cap lookup table generation.
- Beckmann or Disney Diffuse fitting targets.
- GPU acceleration.
- GUI.
- Real-time rendering integration.
- Path-tracing-specific sampling infrastructure.
- Area-light polygon integration.
- Full LUT validation.

Those features belong to subsequent phases.

The required BRDF and LTC sampling functions are mathematical primitives for numerical fitting.

They are not a request to implement a path tracer.

---

## 16. Acceptance Criteria

Phase 2 is complete when:

- [ ] Deterministic midpoint sampling is implemented.
- [ ] BRDF magnitude integration is implemented.
- [ ] Average BRDF direction calculation is implemented.
- [ ] Schlick Fresnel integration is implemented for GGX.
- [ ] Unsupported Fresnel decomposition is handled explicitly.
- [ ] MIS-based cubic error evaluation is implemented.
- [ ] BRDF and LTC PDFs are handled independently.
- [ ] LTC magnitude is excluded from the normalized PDF.
- [ ] Nelder–Mead optimization is implemented.
- [ ] One-dimensional and three-dimensional optimization are supported as required.
- [ ] Parameter constraints prevent invalid LTC matrices.
- [ ] Normal-incidence isotropic fitting is supported.
- [ ] General-view fitting is supported.
- [ ] Explicit initial guesses are supported.
- [ ] Structured fitting results are returned.
- [ ] Optimizer status and diagnostics are available.
- [ ] Single-point GGX fitting works.
- [ ] The `ltcgen fit` CLI command works.
- [ ] JSON output works.
- [ ] Analytical optimizer tests pass.
- [ ] BRDF integration tests pass.
- [ ] MIS error tests pass.
- [ ] GGX fitting tests pass.
- [ ] Independent higher-resolution validation is available.
- [ ] Reference differences are documented.
- [ ] Phase 0 and Phase 1 tests still pass.
- [ ] The project builds without Unreal Engine or GPU dependencies.
- [ ] No LUT grid generator or DDS writer has been introduced.

---

## 17. Implementation Procedure

Implement Phase 2 in the following order:

1. Inspect the Phase 0 and Phase 1 implementation.
2. Review the original LTC fitting and Nelder–Mead reference code.
3. Confirm the BRDF value and PDF contracts.
4. Introduce fitting configuration and result types.
5. Implement deterministic midpoint sampling.
6. Implement BRDF magnitude integration.
7. Implement Fresnel coefficient and average direction calculations.
8. Add BRDF integration tests.
9. Implement the MIS error evaluator.
10. Add synthetic-distribution error tests.
11. Implement the generic Nelder–Mead optimizer.
12. Add analytical optimizer tests.
13. Implement the LTC parameter optimization adapter.
14. Implement the single-point LTC fitting API.
15. Implement the `ltcgen fit` command.
16. Add representative GGX fitting tests.
17. Add independent numerical validation.
18. Compare selected cases with the reference implementation.
19. Run the complete test suite.
20. Update documentation.
21. Report implementation results and known limitations.

Implement incrementally and test each component before integrating the next.

Do not defer numerical validation until the entire fitter is complete.

### Final Report

After implementation, provide:

1. Summary of implemented functionality.
2. Files created and modified.
3. Fitting API and configuration.
4. BRDF integration methodology.
5. MIS error formulation.
6. Nelder–Mead implementation details.
7. LTC parameter constraints and initialization behavior.
8. Reference compatibility findings.
9. Representative GGX fitting results.
10. Numerical validation results.
11. Build and test results.
12. Performance observations.
13. Known limitations.
14. Deviations from the specification and their rationale.
15. Readiness for Phase 3.

**Expected result: A deterministic, tested, CPU-based single-point LTC fitting engine that can approximate GGX using MIS error evaluation and Nelder–Mead optimization, while remaining independent of LUT generation and DDS output.**
