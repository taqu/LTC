# Phase 1 — BRDF and LTC Mathematical Core

## Project: LTC LUT Generator

### Objective

Implement the mathematical foundation required for CPU-based LTC (Linearly Transformed Cosines) fitting.

Phase 1 must introduce:

- A reusable BRDF interface.
- An isotropic GGX BRDF implementation.
- GGX importance sampling and PDF evaluation.
- The LTC distribution and its matrix representation.
- LTC evaluation and direction sampling.
- Deterministic numerical and mathematical validation tests.

The primary reference implementation is:

https://github.com/selfshadow/ltc_code

Relevant reference files:

- `fit/brdf.h`
- `fit/brdf_ggx.h`
- `fit/LTC.h`
- `fit/fitLTC.cpp`

The long-term objective is to generate DDS LUTs for real-time area-light evaluation, including compatibility with Unreal Engine's GGX LTC texture conventions.

**Phase 1 must not implement fitting optimization, LUT generation, or DDS writing.**

---

## 1. Existing Project Integration

Phase 0 established:

- C++20 and CMake infrastructure.
- A standalone `ltcgen` executable.
- A reusable DDS reader.
- DDS metadata and pixel inspection.
- Automated tests using GoogleTest.
- CLI11 and JSON dependencies.

Before implementing Phase 1:

1. Inspect the repository and existing implementation.
2. Reuse established coding conventions and CMake targets.
3. Preserve all Phase 0 functionality.
4. Avoid unnecessary restructuring.
5. Extend the existing test infrastructure.
6. Review the reference implementation's mathematical conventions.

Do not replace working Phase 0 components.

The mathematical core must remain independent of the DDS reader and CLI.

---

## 2. Project Structure

Extend the existing project approximately as follows:

```text
include/ltc/
├── BRDF.h
├── GGX.h
├── LTC.h
├── Math.h
└── Sampling.h

src/
├── brdf/
│   └── GGX.cpp
├── fitting/
│   └── LTC.cpp
└── core/
    ├── Math.cpp
    └── Sampling.cpp

tests/
├── BRDFTests.cpp
├── GGXTests.cpp
├── LTCTests.cpp
├── SamplingTests.cpp
└── NumericalValidationTests.cpp

docs/
├── brdf-interface.md
├── ggx.md
├── ltc-mathematics.md
└── numerical-validation.md
```

Adapt this structure to existing repository conventions.

Do not introduce empty placeholder files.

---

## 3. Mathematical Conventions

Establish explicit conventions before implementing the BRDF and LTC models.

### Coordinate System

Use the following local shading coordinate system:

- Surface normal: `N = (0, 0, 1)`.
- Positive Z represents the upper hemisphere.
- View direction `V` points away from the surface.
- Light direction `L` points away from the surface toward the incident light.
- Both directions are expected to be normalized.
- The half-vector is `H = normalize(V + L)` when valid.

For the initial isotropic fitting model, the view direction may be represented as:

```cpp
Vec3 V(sin(theta), 0.0, cos(theta));
```

Use right-handed vector and matrix conventions and document them explicitly.

### Precision

Use double precision for the mathematical core unless existing project architecture provides a compelling reason otherwise.

However, the original LTC reference implementation primarily uses single-precision floating-point arithmetic.

Design the mathematical layer so that reference-compatible float calculations can be introduced later without rewriting the BRDF and LTC algorithms.

A templated scalar implementation may be used if it remains straightforward and does not introduce excessive complexity.

Prioritize:

1. Mathematical correctness.
2. Reference compatibility.
3. Numerical stability.
4. Maintainability.
5. Performance.

Do not introduce SIMD-specific or platform-specific optimization in Phase 1.

### Matrix Convention

Use a documented column-vector convention:

```text
transformedDirection = M * originalDirection
```

Clearly distinguish mathematical matrix element notation from the storage order used by GLM.

Do not rely on undocumented row-major or column-major assumptions.

---

## 4. Mathematical Utilities

Implement or reuse the following operations:

- Vector normalization.
- Dot product.
- Cross product.
- Vector length.
- Matrix multiplication.
- Matrix inverse.
- Matrix determinant.
- Finite-value validation.
- Safe normalization.
- Orthonormal basis construction.
- Sampling utility functions.

Prefer GLM if it is already used or appropriate for the project.

Do not implement a large custom mathematics library unless necessary.

### Numerical Safety

Handle the following cases explicitly:

- Zero-length vectors.
- Nearly zero-length vectors.
- Invalid normalization.
- Degenerate half-vectors.
- Singular matrices.
- Non-finite inputs.
- Invalid probability densities.

Do not silently replace invalid mathematical results with arbitrary valid-looking values.

Clearly document whether invalid input is rejected, returns zero density, or produces a structured error.

Expected zero-density cases must be distinguishable from programming errors.

---

## 5. Generic BRDF Interface

Implement a reusable interface that allows additional BRDF models to be introduced in later phases.

The interface must not depend on:

- DDS formats.
- CLI arguments.
- LUT dimensions.
- Fitting optimizers.
- Unreal Engine headers.
- Graphics APIs.

### Suggested Interface

```cpp
struct BRDFContext
{
    Vec3 view;
    double alpha;
};

struct BRDFEvaluation
{
    double value;
    double pdf;
};

class IBRDF
{
public:
    virtual ~IBRDF() = default;

    virtual std::string_view name() const = 0;

    virtual BRDFEvaluation evaluate(
        const BRDFContext& context,
        const Vec3& light) const = 0;

    virtual Vec3 sample(
        const BRDFContext& context,
        const Vec2& randomSample) const = 0;
};
```

This interface is a starting point and may be adjusted to fit the repository.

### Evaluation Contract

The `value` field must represent the cosine-weighted BRDF:

\[
f_{\mathrm{target}}(V,L)
=
f_r(V,L)\max(0,N\cdot L)
\]

It must **not** contain the importance-sampling PDF.

The `pdf` field is the probability density with respect to solid angle for the direction returned by the BRDF sampling strategy.

These values must remain independent.

The reference fitting algorithm uses both quantities separately when calculating MIS-based fitting errors.

### BRDF Parameters

Use the physical microfacet roughness parameter `alpha` at the evaluation interface boundary.

Do not silently square it inside the generic BRDF interface.

For GGX LUT generation, perceptual roughness will eventually be converted using:

\[
\alpha=\max(r^2,10^{-5})
\]

That conversion belongs to the future LUT parameterization layer.

Phase 1 should support direct evaluation using `alpha`.

### Required Interface Properties

- Evaluation must not modify BRDF state.
- Sampling must not modify BRDF state.
- Sampling inputs must be deterministic.
- No hidden global random number generators.
- Evaluation and sampling must be usable concurrently.
- PDF units must always be documented.
- Invalid direction handling must be consistent.

The initial interface only needs to support scalar, isotropic BRDFs.

Do not introduce an unnecessary dynamic plugin system.

---

## 6. GGX BRDF Implementation

Implement an isotropic GGX microfacet reflection model.

Use the original LTC fitting reference as the primary compatibility target.

### 6.1 Normal Distribution Function

The GGX normal distribution is:

\[
D_{\mathrm{GGX}}(H)=
\frac{\alpha^2}
{\pi\left((N\cdot H)^2(\alpha^2-1)+1\right)^2}
\]

For the valid upper hemisphere.

Ensure correct handling of:

- `alpha` approaching zero.
- `alpha = 1`.
- Grazing angles.
- Invalid half-vectors.
- Numerical underflow and overflow.

Use an explicit minimum roughness policy consistent with the reference fitting configuration.

### 6.2 Smith Geometry Term

Implement the GGX Smith masking-shadowing term consistent with the reference BRDF.

Do not substitute a visually similar but mathematically different approximation without documenting the difference.

In particular:

- Do not automatically use Schlick-GGX geometry.
- Do not automatically use height-correlated Smith masking.
- Do not substitute visible-normal GGX sampling unless the PDF and reference compatibility implications are explicitly addressed.

The exact reference behavior must be identified and documented.

### 6.3 Fresnel Handling

For the initial fitting-compatible GGX model, support the reference convention in which the specular lobe can be evaluated without applying a material-specific Fresnel coefficient.

The fitting process will later calculate separate amplitude and Schlick Fresnel coefficients.

Do not hardcode a specific material `F0` into the GGX shape distribution.

Document the distinction between:

- Fresnel-free fitting target.
- Physical BRDF with Fresnel.
- Schlick Fresnel reconstruction during rendering.

Do not implement the amplitude integration yet.

### 6.4 Cosine-Weighted Evaluation

The standard microfacet BRDF is:

\[
f_r(V,L)=
\frac{D(H)G(V,L)F(V,H)}
{4(N\cdot V)(N\cdot L)}
\]

For valid reflected directions.

The fitting target is:

\[
f_{\mathrm{target}}(V,L)
=
f_r(V,L)(N\cdot L)
\]

Use the reference Fresnel-free convention when implementing the fitting target.

Do not accidentally apply the cosine term twice.

### 6.5 Importance Sampling

Implement GGX microfacet-normal sampling using two uniformly distributed input values:

```cpp
Vec3 sample(
    const BRDFContext& context,
    const Vec2& u) const;
```

Use the reference-compatible GGX normal distribution sampling strategy.

For a sampled half-vector `H`, calculate the reflected light direction:

\[
L=2(V\cdot H)H-V
\]

Normalize only when necessary to correct numerical error.

Correctly transform the half-vector PDF into the reflected direction PDF:

\[
p_L(L)=
\frac{p_H(H)}
{4|V\cdot H|}
\]

For standard GGX NDF sampling:

\[
p_H(H)=D(H)\max(0,N\cdot H)
\]

Handle invalid or below-surface reflected directions consistently.

A sample may fall outside the valid integration hemisphere. Do not silently resample until a valid direction is obtained, because doing so changes the sampling distribution.

The evaluator must return the correct corresponding density or zero for invalid directions.

### 6.6 GGX Tests

Implement tests for:

- GGX NDF at `alpha = 1`.
- GGX NDF at normal incidence.
- Small roughness.
- High roughness.
- Normal and grazing view directions.
- Isotropic rotational invariance.
- Reciprocity of the underlying reflection BRDF.
- Finite and nonnegative evaluation.
- Correct PDF transformation.
- Correct sampled reflection direction.
- Deterministic sampling.

Compare selected values against an independently implemented reference formula or the published reference code.

Do not use the same helper function to calculate both an expected value and the value under test.

---

## 7. LTC Mathematical Model

Implement a standalone LTC representation supporting:

- Parametric matrix construction.
- Matrix inverse.
- Determinant.
- LTC distribution evaluation.
- LTC direction sampling.
- LTC PDF evaluation.
- Magnitude scaling.

### 7.1 Clamped Cosine Distribution

The base distribution is:

\[
D_{\mathrm{cos}}(\omega)
=
\frac{\max(0,\omega_z)}{\pi}
\]

Its support is the upper hemisphere.

It must integrate to one.

### 7.2 LTC Transformation

Let `M` be an invertible linear transformation.

The LTC distribution is:

\[
D_{\mathrm{LTC}}(\omega;M)
=
D_{\mathrm{cos}}
\left(
\frac{M^{-1}\omega}
{\|M^{-1}\omega\|}
\right)
\frac{|\det(M^{-1})|}
{\|M^{-1}\omega\|^3}
\]

The amplitude-scaled LTC evaluation is:

\[
f_{\mathrm{LTC}}(\omega)
=
A\,D_{\mathrm{LTC}}(\omega;M)
\]

where `A` is a scalar magnitude.

**The normalized LTC PDF must not include the magnitude.**

Expose these as separate operations.

Suggested API:

```cpp
class LTC
{
public:
    void setParameters(
        double m11,
        double m22,
        double m13);

    void setBasis(
        const Vec3& x,
        const Vec3& y,
        const Vec3& z);

    double pdf(const Vec3& direction) const;

    double evaluate(const Vec3& direction) const;

    Vec3 sample(const Vec2& u) const;

    const Mat3& matrix() const;
    const Mat3& inverseMatrix() const;

    double determinant() const;

    void setMagnitude(double magnitude);
};
```

Adjust the API where necessary for safety, const-correctness, and maintainability.

### 7.3 Matrix Parameterization

Implement the reference-compatible parameterization using:

- `m11`
- `m22`
- `m13`
- Orthonormal basis vectors `X`, `Y`, `Z`

The transformation is represented by:

\[
M =
[X\ Y\ Z]
\begin{bmatrix}
m_{11}&0&m_{13}\\
0&m_{22}&0\\
0&0&1
\end{bmatrix}
\]

Here, `X`, `Y`, and `Z` are the columns of the orthonormal basis matrix.

Be especially careful with GLM constructor ordering.

Verify that the implementation matches the reference LTC parameterization rather than merely matching a visually similar matrix.

The initial LTC state should represent the identity transformation.

### 7.4 Matrix Validation

Validate:

- Finite matrix coefficients.
- Nonzero determinant.
- Positive scale parameters where required.
- Valid orthonormal basis.
- Matrix-inverse consistency.

Do not return an apparently valid LTC distribution when the matrix is singular.

Invalid parameter updates should be explicitly rejected.

Cached matrix, inverse, and determinant values must remain synchronized.

### 7.5 LTC Sampling

Sample the cosine-weighted hemisphere using two uniform variables.

Transform the sampled direction using `M`:

\[
\omega=
\frac{M\omega_0}{\|M\omega_0\|}
\]

where:

\[
\omega_0\sim D_{\mathrm{cos}}
\]

For example:

\[
\theta=\arccos(\sqrt{u_1})
\]

\[
\phi=2\pi u_2
\]

\[
\omega_0=
(\sin\theta\cos\phi,\,
\sin\theta\sin\phi,\,
\cos\theta)
\]

Use a sampling implementation consistent with the reference.

Ensure deterministic behavior for identical inputs.

### 7.6 Support and Normalization

The LTC distribution is normalized over the transformed cosine hemisphere.

Do not assume that the transformed distribution has support only where world/local `z` is positive.

An arbitrary transformed LTC lobe may extend below the original surface hemisphere.

This distinction is important:

- The LTC PDF is normalized over its transformed support.
- The BRDF target is generally defined over the surface upper hemisphere.
- The later fitting process compares these distributions over their relevant directional domain.

Do not silently renormalize LTC over the surface hemisphere.

That would change the reference distribution and invalidate the later fitting algorithm.

---

## 8. Numerical Validation

Create numerical validation tests independent of future fitting optimization.

### 8.1 Cosine Distribution Normalization

Verify:

\[
\int_{\Omega^+}
D_{\mathrm{cos}}(\omega)\,d\omega=1
\]

Use a deterministic numerical integration method.

### 8.2 LTC Normalization

Verify:

\[
\int_{S^2}
D_{\mathrm{LTC}}(\omega)\,d\omega=1
\]

Test multiple valid matrices, including:

- Identity.
- Anisotropic scaling.
- Off-diagonal shear.
- Rotated basis.
- Combinations of rotation and scaling.

Use full-sphere integration when appropriate.

Do not integrate only the upper hemisphere unless the specific matrix guarantees upper-hemisphere support.

### 8.3 Identity LTC

For the identity transformation and unit magnitude:

\[
D_{\mathrm{LTC}}(\omega)=
D_{\mathrm{cos}}(\omega)
\]

Verify this for multiple directions.

This should be one of the primary correctness tests.

### 8.4 Sampling and PDF Consistency

Verify that directions sampled from LTC follow the corresponding PDF.

Use deterministic samples and statistical comparisons.

Test:

- Identity LTC.
- Scaled LTC.
- Sheared LTC.
- Rotated LTC.

An independent numerical integration method should be used to avoid circular tests.

### 8.5 Transformation Invariance

For a positive uniform scale factor `s`:

\[
M'=sM
\]

the resulting normalized directional distribution should remain unchanged.

Verify:

\[
D_{\mathrm{LTC}}(\omega;M')
=
D_{\mathrm{LTC}}(\omega;M)
\]

This test should exercise the general matrix evaluation path. The fitting parameterization may fix one matrix scale to remove this redundant degree of freedom.

### 8.6 Magnitude Independence

Verify that changing the LTC magnitude:

- Changes the amplitude-scaled evaluation.
- Does not change the PDF.
- Does not change sampled directions.

### 8.7 Numerical Edge Cases

Test:

- Directions at the distribution boundary.
- Directions outside the distribution support.
- Very small valid scales.
- Near-singular matrices.
- Invalid matrices.
- Zero-length directions.
- Non-finite values.
- Sampling values at or near the domain boundaries.

Establish reasonable numerical tolerances and document why they are appropriate.

Avoid fragile tests that require exact floating-point equality.

---

## 9. Reference Compatibility

The official implementation uses a particular LTC parameterization and fitting convention.

Phase 1 must document and verify compatibility with:

- GGX evaluation.
- GGX sampling.
- GGX sampling PDF.
- LTC parameter matrix construction.
- LTC distribution evaluation.
- LTC sampling.
- Magnitude semantics.

Implement fixed numerical reference cases when practical.

For example:

```text
GGX:
    alpha = 0.25
    V = normalized(0.6, 0.0, 0.8)
    L = normalized(0.3, 0.4, 0.866)

LTC:
    m11 = 0.8
    m22 = 1.2
    m13 = 0.15
    magnitude = 1.0
```

These are illustrative test inputs, not expected results.

Generate expected values independently from the authoritative reference implementation or explicit analytical calculations.

Do not fabricate reference outputs.

### Compatibility Considerations

Do not silently replace reference formulas with:

- Alternative GGX distributions.
- Different Smith masking models.
- VNDF sampling.
- Alternative roughness conventions.
- Different LTC matrix normalization.
- Different Jacobian calculations.

Improvements can be introduced in future quality modes, but must remain separate from reference-compatible behavior.

---

## 10. Build System Integration

Integrate the new modules into the existing CMake project.

Requirements:

- Keep reusable mathematical components in a library target.
- Keep the CLI separate.
- Avoid circular dependencies.
- Extend existing GoogleTest coverage.
- Preserve Windows/MSVC support.
- Maintain Linux/GCC and Clang compatibility.
- Avoid GPU dependencies.
- Avoid introducing platform-specific numerical APIs.

Suggested logical dependencies:

```text
Math / Sampling
       |
       +----> GGX BRDF
       |
       +----> LTC Distribution
```

Both components should be independently usable.

Future Phase 2 fitting code will depend on both.

Neither should depend on the fitting optimizer.

---

## 11. Documentation

Update the existing documentation to include the Phase 1 implementation.

### docs/brdf-interface.md

Document:

- BRDF coordinate conventions.
- Cosine-weighted evaluation.
- PDF semantics.
- Sampling requirements.
- Roughness parameter conventions.
- How to implement another BRDF.

### docs/ggx.md

Document:

- GGX distribution.
- Smith geometry term.
- Fresnel handling.
- Importance-sampling method.
- Direction-space PDF transformation.
- Reference compatibility considerations.

### docs/ltc-mathematics.md

Document:

- Clamped Cosine Distribution.
- LTC transformation.
- Matrix parameterization.
- Jacobian.
- PDF and magnitude separation.
- Sampling.
- Matrix conventions.
- Support of transformed distributions.

### docs/numerical-validation.md

Document:

- Mathematical invariants.
- Test methodology.
- Numerical integration strategy.
- Expected tolerances.
- Known numerical limitations.
- Reference comparison procedures.

Update README.md to describe the new mathematical core.

Do not advertise LUT generation as implemented functionality.

---

## 12. Scope Restrictions

Phase 1 must not implement:

- Nelder–Mead optimization.
- LTC fitting.
- MIS fitting-error evaluation.
- Average BRDF direction integration.
- Magnitude/Fresnel LUT precomputation.
- LUT parameter-grid generation.
- DDS writing.
- GGX LUT generation.
- Area-light integration.
- Unreal Engine shader integration.
- GPU acceleration.
- Multithreaded fitting.
- Additional BRDF implementations.
- Path-tracing-specific LTC features.

The `sample()` methods are required mathematical primitives for future CPU fitting.

Their presence does not mean that path-tracing features should be implemented.

---

## 13. Acceptance Criteria

Phase 1 is complete when:

- [ ] A reusable BRDF interface exists.
- [ ] Isotropic GGX evaluation is implemented.
- [ ] GGX importance sampling is implemented.
- [ ] GGX sampling PDFs are correct.
- [ ] The GGX model follows documented reference conventions.
- [ ] A reusable LTC representation exists.
- [ ] LTC matrix construction is correct.
- [ ] LTC matrix inversion and determinant handling are correct.
- [ ] LTC PDF evaluation is implemented.
- [ ] LTC magnitude is independent from the PDF.
- [ ] LTC direction sampling is implemented.
- [ ] Identity LTC matches the Clamped Cosine Distribution.
- [ ] LTC normalization tests pass.
- [ ] GGX numerical validation tests pass.
- [ ] Invalid and degenerate inputs are handled safely.
- [ ] All tests are deterministic.
- [ ] Phase 0 DDS inspection functionality remains working.
- [ ] The project builds without Unreal Engine or a GPU SDK.
- [ ] Documentation accurately describes the implemented math.
- [ ] No fitting optimizer or LUT generator is introduced prematurely.

---

## 14. Implementation Procedure

Follow this implementation order:

1. Inspect the existing Phase 0 repository.
2. Review the reference GGX and LTC implementation.
3. Establish coordinate, matrix, and precision conventions.
4. Implement mathematical utilities.
5. Implement the generic BRDF interface.
6. Implement GGX evaluation and sampling.
7. Add GGX unit and reference tests.
8. Implement LTC matrix construction.
9. Implement LTC evaluation and PDF calculation.
10. Implement LTC direction sampling.
11. Add LTC mathematical and numerical validation tests.
12. Integrate the new modules into CMake.
13. Run all tests, including Phase 0 tests.
14. Update documentation.
15. Report the results.

Implement incrementally and verify correctness after each major component.

Do not defer all testing until the end.

### Final Report

After implementation, provide:

1. Summary of implemented functionality.
2. Files created or modified.
3. Mathematical conventions adopted.
4. GGX distribution and masking model used.
5. GGX sampling strategy and PDF formula.
6. LTC matrix representation and Jacobian convention.
7. Reference compatibility findings.
8. Numerical test results and tolerances.
9. Build and test results.
10. Known limitations.
11. Any deviations from this specification.
12. Readiness for Phase 2.

**Expected result: A tested, reference-compatible GGX and LTC mathematical core that can be used directly by the Phase 2 numerical fitting engine, without implementing the fitting engine itself.**
