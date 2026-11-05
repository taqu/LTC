# Phase 6 — Generic BRDF Support and Extensible LTC Fitting

## Project: LTC LUT Generator

### Objective

Extend the existing CPU-based LTC LUT Generator from a GGX-focused implementation into a reusable, multi-BRDF fitting framework.

Phase 6 must introduce:

- Beckmann microfacet BRDF support.
- Disney/Burley Diffuse BRDF support.
- A reusable BRDF registration and discovery mechanism.
- Explicit BRDF parameter semantics.
- BRDF-specific sampling and PDF validation.
- Generalized amplitude and Fresnel coefficient handling.
- Generic LUT naming and DDS export profiles.
- End-to-end generation and validation for multiple BRDFs.
- Documentation and examples for implementing custom BRDFs.

The primary architectural requirement is:

**Adding a new compatible BRDF must not require modifying the LTC fitting algorithm, Nelder–Mead optimizer, LUT generator, CPU scheduler, DDS writer, or numerical validation algorithms.**

The long-term application remains real-time area-light evaluation.

Do not implement path-tracing-specific LTC features.

### References

Primary LTC reference:

https://github.com/selfshadow/ltc_code

Relevant files:

- `fit/brdf.h`
- `fit/brdf_ggx.h`
- `fit/brdf_beckmann.h`
- `fit/brdf_disneyDiffuse.h`
- `fit/fitLTC.cpp`

Disney diffuse reference:

https://disneyanimation.com/publications/physically-based-shading-at-disney/

Review the actual reference code and mathematical conventions before implementation.

---

## 1. Existing Project Integration

The previous phases established the following components.

| Phase | Functionality |
|---|---|
| 0 | C++20, CMake, CLI, DDS Reader |
| 1 | GGX BRDF and LTC mathematical core |
| 2 | Single-point numerical fitting |
| 3 | LUT grid generation, parallelization, checkpoints |
| 4 | DDS export and Unreal Engine GGX packing |
| 5 | Reference comparison and area-light numerical validation |

Before making changes:

1. Inspect the existing repository.
2. Identify the actual BRDF interfaces.
3. Review the dependencies between BRDF implementations and the fitting engine.
4. Determine which components currently assume GGX.
5. Identify assumptions concerning roughness, Fresnel coefficients, and amplitude.
6. Review the reference Beckmann and Disney Diffuse implementations.
7. Preserve existing GGX reference compatibility and numerical results.

Do not rewrite working components unnecessarily.

Any required changes to existing interfaces must remain backward-compatible where practical and must include regression tests.

---

## 2. Architectural Requirements

The target architecture is:

```text
                      CLI
                       |
                       v
                  BRDF Registry
                       |
          +------------+-------------+
          |            |             |
          v            v             v
         GGX        Beckmann    Disney Diffuse
          |            |             |
          +------------+-------------+
                       |
                       v
                     IBRDF
                       |
                       v
                  LTC Fitting
                       |
                       v
                  LUT Generator
                       |
                       v
                     LUTData
                       |
          +------------+-------------+
          |                          |
          v                          v
     DDS Exporter             Validation
          |                          |
          v                          v
      DDS Textures             JSON / CSV
```

### Key Requirements

- The fitting engine must use the generic BRDF interface.
- The optimizer must remain completely BRDF-independent.
- The LUT generator must not instantiate BRDF implementations directly.
- BRDF registration must not be coupled to DDS serialization.
- The validation framework must use generic BRDF evaluation.
- Parameter semantics must be explicit.
- Output packing must validate BRDF capabilities.
- Existing GGX results must remain compatible with previous phases.

Do not introduce a dynamic plugin system or shared-library ABI.

For Phase 6, statically registered C++ BRDF implementations are sufficient.

---

## 3. Project Structure

Extend the existing structure approximately as follows:

```text
include/ltc/
├── BRDF.h
├── BRDFCapabilities.h
├── BRDFParameters.h
├── BRDFRegistry.h
├── Beckmann.h
├── DisneyDiffuse.h
└── AmplitudeModel.h

src/
├── brdf/
│   ├── BRDFRegistry.cpp
│   ├── Beckmann.cpp
│   └── DisneyDiffuse.cpp
├── fitting/
│   └── BRDFIntegration.cpp
├── export/
│   └── GenericPacking.cpp
└── cli/
    └── main.cpp

tests/
├── BeckmannTests.cpp
├── DisneyDiffuseTests.cpp
├── BRDFRegistryTests.cpp
├── BRDFCapabilityTests.cpp
├── GenericFittingTests.cpp
├── GenericExportTests.cpp
└── MultiBRDFValidationTests.cpp

docs/
├── brdf-extension.md
├── beckmann.md
├── disney-diffuse.md
└── generic-ltc-export.md

examples/
└── custom-brdf/
    ├── CustomBRDF.h
    ├── CustomBRDF.cpp
    └── README.md
```

Adapt the structure to existing conventions.

Do not introduce unused abstractions or placeholder implementations.

---

## 4. Generic BRDF Interface

Review and extend the existing Phase 1 interface only where necessary.

The interface must support:

- Cosine-weighted BRDF evaluation.
- Directional importance sampling.
- Solid-angle PDF evaluation.
- Parameter definitions.
- BRDF capabilities.
- Optional Fresnel decomposition.
- Immutable and thread-safe evaluation.

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
        const Vec2& u) const = 0;
};
```

This is illustrative and must be reconciled with the current implementation.

### Evaluation Contract

The evaluation must return the cosine-weighted BRDF:

\[
f_{\mathrm{target}}(V,L)
=f_r(V,L)\max(0,N\cdot L)
\]

The PDF must be the solid-angle density associated with the BRDF sampling strategy.

Do not confuse the BRDF value with the sampling PDF.

A BRDF is not required to sample directions proportionally to its evaluation value.

A cosine-weighted proposal distribution is acceptable for diffuse BRDFs, provided its PDF is correct.

### Invalid Directions

Define consistent behavior for:

- Directions below the surface.
- Invalid view directions.
- Degenerate half-vectors.
- Non-finite inputs.
- Zero-probability directions.
- Samples outside the valid integration hemisphere.

Do not silently resample invalid reflection directions, since this changes the effective PDF.

An invalid or zero-contribution sample must not introduce bias into BRDF integration.

### Thread Safety

BRDF implementations must be safe for concurrent read-only evaluation.

Do not use mutable global random generators or shared scratch buffers.

---

## 5. BRDF Parameters and Roughness Semantics

One of the main challenges of generic BRDF fitting is parameter interpretation.

The existing GGX implementation uses a microfacet roughness parameter, typically called `alpha`.

However, not every BRDF uses `alpha` with the same meaning.

### Requirements

Separate:

1. User-facing BRDF parameters.
2. LUT grid parameters.
3. Internal BRDF evaluation parameters.

The fitting engine must operate on a validated BRDF evaluation context.

The parameter grid must convert its coordinates into the appropriate BRDF parameters.

### Suggested Parameter Model

```cpp
enum class BRDFParameterization
{
    MicrofacetAlpha,
    PerceptualRoughness
};

struct BRDFParameterInfo
{
    std::string name;

    double minimum;
    double maximum;
    double defaultValue;
};

struct BRDFCapabilities
{
    bool isotropic = true;
    bool reciprocal = true;

    bool supportsImportanceSampling = true;
    bool supportsSchlickFresnel = false;

    BRDFParameterization parameterization;
};
```

This is a minimal suggested model.

A more general parameter descriptor system is acceptable if justified by actual needs.

Avoid constructing a complex runtime reflection framework.

### GGX

Preserve existing behavior.

The reference LUT mapping uses:

\[
\alpha=\max(r^2,10^{-5})
\]

where \(r\) is perceptual roughness.

### Beckmann

Use microfacet alpha with a clearly documented mapping from perceptual roughness.

For reference-compatible operation, follow the parameterization used by the authoritative LTC Beckmann implementation.

Do not silently substitute GGX distribution formulas.

### Disney Diffuse

The Disney diffuse roughness parameter is a perceptual model parameter.

It is not inherently equivalent to the GGX microfacet alpha parameter.

Implement a BRDF-specific mapping.

Do not blindly square roughness twice.

### Backward Compatibility

Existing GGX commands must continue to behave as before.

For example:

```bash
ltcgen fit \
    --brdf ggx \
    --alpha 0.25 \
    --ndotv 0.8
```

The meaning of `--alpha` must not change.

For new BRDFs, prefer explicit parameter names.

---

## 6. Beckmann BRDF

Implement an isotropic Beckmann microfacet reflection model.

Use the reference LTC Beckmann implementation as the primary compatibility target.

### 6.1 Normal Distribution

The isotropic Beckmann normal distribution is:

\[
D_{\mathrm{Beckmann}}(H)=
\frac{
\exp\left(
-\frac{\tan^2\theta_h}{\alpha^2}
\right)
}{
\pi\alpha^2\cos^4\theta_h
}
\]

for valid upper-hemisphere half-vectors.

Handle grazing angles safely.

Avoid numerically unstable calculations when the half-vector approaches the horizon.

### 6.2 Geometry Term

Implement the masking-shadowing behavior used by the reference Beckmann BRDF.

Do not automatically substitute:

- GGX Smith masking.
- Schlick-GGX approximations.
- Height-correlated GGX geometry.
- An unrelated Beckmann approximation.

Document the exact geometry model used.

If the reference uses an approximation to the Smith function, preserve that behavior in Reference mode.

An independently improved model may be added later under a distinct profile.

### 6.3 Fresnel

Use the fitting-compatible Fresnel-free convention established for GGX where appropriate.

Do not hardcode material-specific `F0`.

The Beckmann implementation may support the same Schlick Fresnel basis decomposition if its target definition matches the required microfacet reflection assumptions.

Expose this capability explicitly.

### 6.4 Importance Sampling

Implement Beckmann microfacet-normal importance sampling.

For the standard isotropic Beckmann NDF sampling method:

\[
\tan^2\theta_h=-\alpha^2\ln(1-u_1)
\]

\[
\phi=2\pi u_2
\]

Construct the half-vector and calculate the reflected direction:

\[
L=2(V\cdot H)H-V
\]

The associated reflected-direction PDF is:

\[
p_L(L)=
\frac{D(H)\max(0,N\cdot H)}
{4|V\cdot H|}
\]

Handle invalid or below-surface reflected directions consistently.

If a different sampling method is used by the reference, reproduce it in Reference mode.

### 6.5 Tests

Test:

- Beckmann NDF values.
- Distribution normalization.
- Roughness behavior.
- Geometry term.
- Normal incidence.
- Grazing incidence.
- Reciprocity.
- Sampling and PDF consistency.
- Deterministic sampling.
- Zero-contribution directions.
- Numerical stability.

Use independently evaluated mathematical expectations where possible.

Compare representative cases against the original LTC Beckmann implementation.

Do not use the same helper implementation to generate both the actual and expected values.

---

## 7. Disney/Burley Diffuse BRDF

Implement the Disney diffuse model as a separate BRDF.

The initial implementation must target the Disney/Burley diffuse lobe, not the complete Disney Principled BRDF.

Do not implement metallic, specular, clearcoat, sheen, or subsurface components in this phase.

### 7.1 Diffuse Model

The Disney diffuse term is:

\[
f_d(V,L)=
\frac{1}{\pi}
\left[1+(F_{D90}-1)F_L\right]
\left[1+(F_{D90}-1)F_V\right]
\]

where:

\[
F_L=(1-N\cdot L)^5
\]

\[
F_V=(1-N\cdot V)^5
\]

and:

\[
F_{D90}=0.5+
2r(L\cdot H)^2
\]

Here:

- \(r\) is Disney perceptual roughness.
- \(H=\operatorname{normalize}(V+L)\).
- The base color is fixed to unit reflectance for scalar LUT fitting.

The cosine-weighted fitting target is:

\[
f_{\mathrm{target}}(V,L)=
f_d(V,L)\max(0,N\cdot L)
\]

Verify the formula against the chosen reference implementation, including its conventions and numerical boundary handling.

### 7.2 Roughness

Disney diffuse uses perceptual roughness directly in the diffuse model.

Do not replace it with `alpha = roughness²` unless the selected reference implementation explicitly defines that behavior.

The grid-to-BRDF parameter mapping must be separate from the GGX mapping.

### 7.3 Importance Sampling

Use cosine-weighted hemisphere sampling as the initial importance-sampling proposal.

The corresponding PDF is:

\[
p(L)=\frac{\max(0,N\cdot L)}{\pi}
\]

This sampling strategy does not exactly reproduce the Disney diffuse directional distribution.

That is acceptable because the fitting engine uses importance sampling with the correct PDF.

Do not incorrectly return the Disney diffuse BRDF evaluation as its sampling PDF.

### 7.4 Fresnel Coefficients

The Disney diffuse model contains Schlick-like factors, but these are part of its diffuse model.

They must not be confused with the GGX specular Schlick Fresnel decomposition.

For the generic Disney diffuse implementation:

```text
supportsSchlickFresnel = false
```

The fitting process must still calculate:

- Magnitude.
- Average direction.
- LTC shape.

Do not calculate or export a meaningless GGX-style Fresnel coefficient.

### 7.5 Tests

Test:

- Disney diffuse at normal incidence.
- Perceptual roughness endpoints.
- Grazing view directions.
- Grazing light directions.
- Reciprocity.
- Nonnegative BRDF evaluation.
- Deterministic cosine sampling.
- Cosine PDF normalization.
- Correct PDF reporting.
- Magnitude integration.
- Average direction.
- Stable LTC fitting.

Compare selected evaluations with published Disney/Burley equations and the original LTC Disney Diffuse implementation.

---

## 8. Generalized Amplitude Handling

The existing GGX implementation calculates:

- Magnitude.
- Schlick Fresnel coefficient.

These must no longer be assumed to exist for every BRDF.

### Requirements

The fitting engine must always support a scalar magnitude:

\[
A(V,p)=
\int_{\Omega^+}
f_{\mathrm{target}}(V,L,p)
\,d\omega_L
\]

where \(p\) represents the BRDF parameters.

Additional amplitude coefficients are model-dependent.

### Suggested Model

```cpp
enum class AmplitudeModel
{
    Scalar,
    SchlickFresnel
};

struct AmplitudeResult
{
    double magnitude = 0.0;

    std::optional<double> fresnelCoefficient;
};
```

Alternative representations are acceptable if they preserve the same semantics.

### Model Behavior

For GGX:

```text
AmplitudeModel = SchlickFresnel
```

For Beckmann:

```text
AmplitudeModel = SchlickFresnel
```

Only if the implemented fitting target supports the same decomposition.

For Disney Diffuse:

```text
AmplitudeModel = Scalar
```

### Important Constraints

- Do not interpret absent Fresnel data as zero Fresnel.
- Do not force every BRDF into the GGX amplitude equation.
- Do not modify the normalized LTC PDF based on magnitude.
- Do not introduce material-specific color into scalar BRDF fitting.
- Preserve existing GGX numerical behavior.

If the intermediate LUT format requires extension, version it appropriately.

Old GGX `.ltcbin` files and checkpoints should remain readable where practical.

If a format migration is necessary, provide clear error reporting and document the compatibility policy.

---

## 9. BRDF Registration and Discovery

Introduce a lightweight BRDF registry.

The registry must allow the CLI and LUT generator to select implementations by name without hardcoding BRDF construction into the fitting engine.

### Suggested API

```cpp
class BRDFRegistry
{
public:
    using Factory =
        std::function<std::unique_ptr<IBRDF>()>;

    void registerBRDF(
        std::string name,
        Factory factory);

    std::unique_ptr<IBRDF> create(
        std::string_view name) const;

    std::vector<std::string> names() const;
};
```

Adapt this to the current repository.

### Requirements

- Register built-in BRDF implementations.
- Reject duplicate names.
- Return useful errors for unknown names.
- Support listing available BRDFs.
- Avoid global mutable registration state where practical.
- Keep registration independent from fitting and DDS modules.
- Use deterministic registration ordering.
- Support future custom BRDF registration.

Register:

```text
ggx
beckmann
disney-diffuse
```

### CLI Discovery

Implement or extend:

```bash
ltcgen list-brdfs
```

Example output:

```text
Available BRDFs:

ggx
beckmann
disney-diffuse
```

Support detailed information:

```bash
ltcgen list-brdfs --details
```

Include:

- BRDF name.
- Parameter names.
- Valid parameter ranges.
- Isotropic capability.
- Amplitude model.
- Available sampling strategy.

The registry must not require any changes to `LTCFitter` when adding new BRDF types.

---

## 10. Generic Fitting Integration

Integrate Beckmann and Disney Diffuse with the Phase 2 fitting engine.

### Requirements

For each BRDF:

1. Resolve its parameter values.
2. Evaluate the BRDF integration terms.
3. Determine the average direction.
4. Construct the LTC orientation.
5. Initialize the shape parameters.
6. Optimize using the existing MIS error function.
7. Return the fitted LTC and amplitude.
8. Preserve numerical diagnostics.

Do not create BRDF-specific optimizer implementations.

The existing Nelder–Mead optimizer must remain unchanged unless a genuine correctness problem is found.

### Zero-Energy Handling

Some BRDF configurations or custom models may produce zero or negligible integrated energy.

The fitter must:

- Detect zero-energy results.
- Avoid normalizing a zero average direction.
- Avoid meaningless optimization.
- Return an explicit status.
- Preserve correct zero magnitude.

Do not fabricate a directional lobe with an arbitrary nonzero magnitude.

### Sampling Requirements

The generic MIS evaluator must not assume that BRDF importance sampling is identical to its normalized evaluation distribution.

It must use:

- The actual BRDF sampling PDF.
- The normalized LTC PDF.
- The cosine-weighted BRDF target value.
- The magnitude-scaled LTC value.

Verify that all methods operate in consistent solid-angle units.

### Testing

Run single-point fitting for:

- GGX.
- Beckmann.
- Disney Diffuse.

Verify:

- Finite coefficients.
- Valid transformation matrices.
- Valid magnitude.
- Correct optional Fresnel behavior.
- Error reduction.
- Deterministic results.
- No BRDF-specific changes to the optimizer.

---

## 11. Generic LUT Generation

Extend the Phase 3 LUT generator to work with registered BRDFs.

### Parameter Grid

The grid must map its normalized coordinates to the selected BRDF parameters.

Use a BRDF-specific parameter mapping policy rather than hardcoded GGX alpha conversion.

For isotropic BRDFs, preserve the existing two-dimensional structure:

```text
X: Roughness-like parameter
Y: View angle
```

The exact meaning of the X coordinate must be described in LUT metadata.

### Default Resolution

Use:

```text
64 × 64
```

as the default resolution for all initial BRDFs.

Allow configurable dimensions.

### Traversal

Reuse the established reference-compatible traversal:

- High roughness to low roughness.
- Normal incidence to grazing incidence.
- Previous roughness row initialization at normal incidence.
- Previous view-angle result initialization within the row.

Do not change the dependency-aware CPU scheduler.

### Parallelization

Verify that all BRDF implementations are safe under parallel fitting.

Use read-only BRDF instances or worker-local instances as appropriate.

Do not introduce shared mutable sampling state.

### Checkpointing

Extend checkpoint metadata to include:

- BRDF identifier.
- BRDF parameter mapping.
- BRDF implementation version.
- Amplitude model.
- Applicable fitting configuration.

Reject resumes using incompatible BRDF configurations.

Do not permit a checkpoint generated for GGX to resume as Beckmann or Disney Diffuse.

---

## 12. Generic DDS Export

The existing Unreal Engine output profile must remain unchanged for GGX.

### Unreal Profile

Preserve:

```text
GGX_LTCMat.dds
GGX_LTCAmp.dds
```

Do not change their mathematical semantics, format, naming, or packing as part of generic BRDF support.

Do not claim that Beckmann or Disney Diffuse output is directly compatible with Unreal Engine's GGX shader.

### Generic Profile

Introduce or extend:

```text
generic
```

The generic profile must support:

- A four-component LTC matrix texture.
- One or more amplitude coefficients.
- Explicit amplitude encoding.
- BRDF-specific file naming.
- Parameter mapping metadata.

Example outputs:

```text
Beckmann_LTCMat.dds
Beckmann_LTCAmp.dds

DisneyDiffuse_LTCMat.dds
DisneyDiffuse_LTCAmp.dds
```

The matrix texture should use RGBA16F by default.

The amplitude texture may use RG16F when the profile requires two channels.

For a scalar-only amplitude model, define an explicit documented layout, for example R16F if supported by the DDS infrastructure.

Do not silently fill an unused channel with zero and imply it represents a valid Fresnel coefficient.

If R16F support is not implemented, an RG16F container with a documented unused channel is acceptable.

The unused channel must be clearly marked in metadata.

### Generic Runtime Contract

Provide documentation explaining how a renderer should decode each generated texture.

The generic profile is not automatically compatible with existing Unreal Engine shader code.

---

## 13. CLI Integration

Extend the existing CLI without breaking previous commands.

### List BRDFs

```bash
ltcgen list-brdfs
```

### Beckmann Single-Point Fitting

```bash
ltcgen fit \
    --brdf beckmann \
    --alpha 0.25 \
    --ndotv 0.8 \
    --profile reference
```

### Disney Diffuse Single-Point Fitting

```bash
ltcgen fit \
    --brdf disney-diffuse \
    --roughness 0.5 \
    --ndotv 0.8 \
    --profile reference
```

### Beckmann LUT Generation

```bash
ltcgen generate \
    --brdf beckmann \
    --width 64 \
    --height 64 \
    --threads 8 \
    --output ./beckmann.ltcbin
```

### Disney Diffuse LUT Generation

```bash
ltcgen generate \
    --brdf disney-diffuse \
    --width 64 \
    --height 64 \
    --threads 8 \
    --output ./disney-diffuse.ltcbin
```

### Generic DDS Export

```bash
ltcgen export \
    --input ./beckmann.ltcbin \
    --profile generic \
    --output ./output/beckmann
```

### Numerical Validation

```bash
ltcgen validate distribution \
    --input ./beckmann.ltcbin \
    --samples 65536 \
    --report ./reports/beckmann.json
```

```bash
ltcgen validate lighting \
    --input ./output/beckmann \
    --profile generic \
    --scenarios ./validation/scenarios \
    --report ./reports/beckmann-lighting.json
```

Update the option parser to support BRDF-specific parameters.

Unknown or unsupported parameters must produce clear errors.

Do not silently reinterpret `--alpha` as Disney perceptual roughness.

---

## 14. Numerical Validation

Reuse the Phase 5 validation framework.

### Beckmann Validation

Verify:

- NDF normalization.
- Masking-shadowing behavior.
- Sampling PDF consistency.
- BRDF reciprocity.
- Magnitude integration.
- Fitting error reduction.
- LUT continuity.
- DDS reconstruction.
- Area-light integration accuracy.

### Disney Diffuse Validation

Verify:

- Formula correctness.
- Roughness parameter handling.
- Reciprocity.
- Cosine-weighted sampling.
- Sampling PDF normalization.
- Integrated magnitude.
- Optional Fresnel absence.
- Fitting error reduction.
- LUT interpolation.
- Area-light integration accuracy.

### Reference Comparison

Compare against the original LTC reference implementation where suitable reference data can be generated.

Do not fabricate golden data.

Reference compatibility must specify:

- BRDF model.
- Roughness parameterization.
- Sampling strategy.
- Geometry term.
- Precision.
- Fitting configuration.
- Export profile.

If the reference conventions differ from the generic framework, document the differences rather than silently changing the model.

### Quality Metrics

For each BRDF, record:

- Converged cells.
- Maximum-iteration cells.
- Failed cells.
- Mean fitting error.
- Maximum fitting error.
- Independent distribution error.
- Worst lighting error.
- Median lighting error.
- DDS quantization error.
- LUT discontinuities.

Use the established Phase 5 metrics and report formats.

Do not introduce incomparable BRDF-specific quality scores.

### Approximation Limitations

A single LTC lobe may approximate different BRDFs with different accuracy.

Do not require Beckmann and Disney Diffuse to achieve exactly the same numerical error as GGX.

A valid fit can have a larger approximation error due to inherent distribution differences.

Report this separately from implementation defects.

---

## 15. Custom BRDF Example

Provide a minimal working example demonstrating how to implement and register a new BRDF.

Use a simple Lambertian BRDF as the example.

For unit reflectance:

\[
f_r(V,L)=\frac{1}{\pi}
\]

The cosine-weighted target is:

\[
f_{\mathrm{target}}(V,L)=
\frac{\max(0,N\cdot L)}{\pi}
\]

Use cosine-weighted hemisphere sampling.

### Required Demonstration

The example must show:

1. Implementing the BRDF interface.
2. Evaluating the cosine-weighted BRDF.
3. Returning the correct sampling PDF.
4. Implementing deterministic direction sampling.
5. Describing BRDF parameters and capabilities.
6. Registering the BRDF.
7. Running single-point fitting.
8. Generating an LTC LUT.
9. Exporting DDS textures.
10. Running distribution and lighting validation.

### Analytical Property

The normalized Lambertian distribution is already a clamped cosine distribution.

Therefore, an identity LTC with unit magnitude represents it exactly.

Use this property as an integration test.

This test should verify the complete generic BRDF pipeline, not just the Lambertian evaluator.

The example must not require changes to the existing fitting engine.

---

## 16. Automated Tests

Extend GoogleTest and CTest.

### BRDF Unit Tests

Test Beckmann and Disney Diffuse independently.

Use analytical reference values and numerical integration where appropriate.

### Registry Tests

Verify:

- Built-in registration.
- Name lookup.
- Duplicate registration rejection.
- Unknown name handling.
- Deterministic listing.
- Factory construction.

### Generic Fitting Tests

Verify that the same fitting API works for:

- GGX.
- Beckmann.
- Disney Diffuse.
- Custom Lambertian BRDF.

### Amplitude Tests

Verify:

- GGX Schlick coefficients.
- Beckmann Schlick coefficients when supported.
- Scalar-only Disney Diffuse amplitude.
- Unsupported decomposition handling.
- Zero-energy behavior.
- Correct serialization and deserialization.

### LUT Generation Tests

Generate small LUTs for all BRDFs.

Verify:

- Parameter mapping.
- Complete cell generation.
- Deterministic initialization.
- Parallel consistency.
- Correct checkpoint metadata.
- Correct resume behavior.

### DDS Tests

Verify:

- Correct filenames.
- Correct matrix packing.
- Correct amplitude packing.
- Correct format metadata.
- DDS round-trip decoding.
- No false Unreal compatibility claims.

### Lighting Tests

Run representative area-light validation scenarios for each BRDF.

Verify numerical convergence and report approximation errors independently from serialization errors.

### Regression Tests

Preserve all GGX reference tests.

Existing GGX fitting and output should not change unexpectedly.

If a necessary correctness fix changes results, document the change and provide a numerical comparison.

Do not silently regenerate golden baselines.

---

## 17. Performance and Thread Safety

The project remains CPU-only.

Do not introduce GPU dependencies.

### Requirements

- BRDF evaluation must be thread-safe.
- Sampling must be deterministic.
- No shared mutable random state.
- Avoid unnecessary allocations in evaluation loops.
- Reuse existing parallel LUT scheduling.
- Preserve deterministic cell initialization.
- Keep validation output ordering stable.

### Performance Measurements

Measure generation performance for:

- GGX.
- Beckmann.
- Disney Diffuse.

For representative configurations, report:

- Total generation time.
- Average fitting time per cell.
- Objective evaluation count.
- Convergence rate.
- CPU scaling across worker counts.

Do not impose identical performance requirements on different BRDFs.

---

## 18. Documentation

Update the following documentation.

### docs/brdf-extension.md

Describe:

- The generic BRDF interface.
- Evaluation semantics.
- Sampling/PDF requirements.
- Parameter descriptors.
- Capability flags.
- Amplitude models.
- Registration.
- Thread safety.
- How to implement a custom BRDF.

### docs/beckmann.md

Describe:

- Beckmann NDF.
- Geometry term.
- Sampling method.
- PDF transformation.
- Roughness parameterization.
- Reference compatibility.
- Numerical behavior.

### docs/disney-diffuse.md

Describe:

- Disney/Burley diffuse formula.
- Roughness interpretation.
- Cosine-weighted sampling.
- Scalar amplitude handling.
- Known limitations.
- Reference compatibility.

### docs/generic-ltc-export.md

Describe:

- Generic DDS filenames.
- Matrix texture format.
- Amplitude texture layout.
- Supported BRDF capabilities.
- Parameter mapping.
- Shader decoding requirements.
- Unreal compatibility restrictions.

### README.md

Add:

- Supported BRDF list.
- Beckmann generation example.
- Disney Diffuse generation example.
- Generic DDS export example.
- Custom BRDF extension instructions.
- Numerical validation examples.

Keep existing GGX documentation accurate.

---

## 19. Scope Restrictions

Phase 6 must not implement:

- Anisotropic BRDF fitting.
- Multiple LTC lobes.
- GGX multiple-scattering compensation.
- Complete Disney Principled BSDF.
- Sheen importance-sampling LUT generation.
- Path-tracing infrastructure.
- GPU fitting.
- GPU rendering.
- Unreal Engine shader modifications.
- Dynamic library plugins.
- Runtime scripting.
- New numerical optimizers.
- Automatic BRDF code generation.
- Automatic LUT smoothing.
- GUI.

Do not expand the scope beyond the multi-BRDF architecture and the two additional BRDF implementations.

---

## 20. Acceptance Criteria

Phase 6 is complete when:

- [ ] Beckmann BRDF evaluation is implemented.
- [ ] Beckmann importance sampling and PDFs are implemented.
- [ ] Disney Diffuse evaluation is implemented.
- [ ] Disney Diffuse sampling and PDFs are implemented.
- [ ] BRDF parameter semantics are explicit.
- [ ] GGX alpha behavior remains unchanged.
- [ ] Disney perceptual roughness is not incorrectly squared.
- [ ] A reusable BRDF registry exists.
- [ ] BRDF capability information is available.
- [ ] Scalar and Schlick amplitude models are supported.
- [ ] Unsupported Fresnel decomposition is handled correctly.
- [ ] The fitting engine supports all built-in BRDFs.
- [ ] The optimizer remains BRDF-independent.
- [ ] The LUT generator supports all built-in BRDFs.
- [ ] Existing dependency-aware CPU parallelization works.
- [ ] Checkpoints include BRDF identity and configuration.
- [ ] Generic DDS export works.
- [ ] Unreal GGX export remains backward-compatible.
- [ ] Reference comparisons are available where authoritative data exists.
- [ ] Phase 5 distribution validation works for all BRDFs.
- [ ] Area-light integration validation works for all BRDFs.
- [ ] A working custom Lambertian BRDF example is provided.
- [ ] Custom BRDF registration requires no fitter changes.
- [ ] All Phase 0–5 regression tests pass.
- [ ] Documentation is updated.
- [ ] The project remains C++20, CPU-only, and CLI-based.

---

## 21. Implementation Procedure

Implement Phase 6 incrementally.

### Stage A — Generic BRDF Infrastructure

1. Inspect existing BRDF assumptions.
2. Define parameter and capability semantics.
3. Introduce the BRDF registry.
4. Generalize amplitude representation.
5. Preserve GGX behavior with regression tests.

### Stage B — Beckmann

6. Implement Beckmann NDF.
7. Implement the reference-compatible geometry term.
8. Implement importance sampling and PDF evaluation.
9. Add independent mathematical tests.
10. Verify single-point LTC fitting.
11. Compare with reference implementation.

### Stage C — Disney Diffuse

12. Implement the Disney/Burley diffuse formula.
13. Implement its perceptual roughness mapping.
14. Implement cosine-weighted sampling.
15. Add mathematical and PDF tests.
16. Verify scalar-amplitude fitting.
17. Compare with reference implementation.

### Stage D — LUT and DDS Integration

18. Integrate both BRDFs with the LUT generator.
19. Extend checkpoint metadata as required.
20. Implement generic DDS packing.
21. Add CLI selection and parameter handling.
22. Generate small LUTs for both BRDFs.
23. Generate full-resolution LUTs.
24. Verify parallel and resumed generation.

### Stage E — Validation and Extensibility

25. Run distribution validation.
26. Run DDS reconstruction validation.
27. Run area-light numerical validation.
28. Implement the custom Lambertian example.
29. Verify registration without fitter modifications.
30. Run GGX regression comparisons.
31. Benchmark CPU generation.
32. Update documentation.
33. Produce the implementation report.

Do not proceed to full-resolution LUT generation before the new BRDF evaluators and PDFs pass mathematical validation.

---

## 22. Final Implementation Report

After implementation, provide:

1. Summary of implemented features.
2. Files created and modified.
3. BRDF interface and registry design.
4. Beckmann mathematical conventions.
5. Disney Diffuse mathematical conventions.
6. BRDF parameter mapping details.
7. Amplitude model changes.
8. Single-point fitting results.
9. Full LUT generation results.
10. DDS output formats and channel semantics.
11. Numerical validation metrics by BRDF.
12. Reference compatibility findings.
13. CPU performance measurements.
14. GGX regression test results.
15. Custom BRDF extension example.
16. Known limitations.
17. Deviations from the specification and their rationale.
18. Readiness for Phase 7.

**Expected result: A fully functional, extensible C++20 CPU-based LTC fitting framework supporting GGX, Beckmann, Disney Diffuse, and custom BRDFs through a shared fitting, LUT generation, DDS export, and numerical validation pipeline, without introducing BRDF-specific changes to the core algorithms.**
