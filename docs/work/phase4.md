# Phase 4 — DDS Export and Unreal Engine LTC Compatibility

## Project: LTC LUT Generator

### Objective

Implement DDS texture generation for the CPU-based LTC LUT Generator.

Phase 4 must convert the full-precision LTC coefficient tables produced by Phase 3 into GPU-ready DDS textures suitable for real-time area-light evaluation.

The primary output targets are:

- `GGX_LTCMat.dds`
- `GGX_LTCAmp.dds`

The implementation must provide:

- Reusable LTC coefficient packing.
- Reference-compatible LTC inverse-matrix normalization.
- Unreal Engine-oriented texture layouts.
- IEEE 754 FP16 encoding.
- FP16 and FP32 DDS writing.
- Integration with the existing DDS Reader.
- Export from previously generated LUT data.
- DDS round-trip validation.
- Reference texture comparison.
- Automated tests and documentation.

The primary mathematical reference is:

https://github.com/selfshadow/ltc_code

Relevant reference files:

- `fit/fitLTC.cpp`
- `fit/export.h`
- `fit/dds.cpp`
- `fit/float_to_half.h`

**Phase 4 must focus on LTC packing, DDS serialization, and compatibility validation. Do not change the underlying fitting algorithm unless a demonstrated correctness defect requires a fix.**

---

## 1. Existing Project Integration

The previous phases established the following functionality.

### Phase 0 — Foundation

- C++20 and CMake infrastructure.
- CLI application.
- DDS Reader.
- DDS header inspection.
- FP16/FP32 pixel decoding.
- Automated tests.

### Phase 1 — Mathematical Core

- Generic BRDF interface.
- GGX BRDF evaluation.
- LTC distribution representation.
- LTC matrices and sampling.

### Phase 2 — Fitting Engine

- BRDF energy integration.
- Schlick Fresnel coefficient integration.
- MIS fitting error.
- Nelder–Mead optimization.
- Single-point LTC fitting.

### Phase 3 — LUT Generation

- Two-dimensional parameter grids.
- Reference-compatible fitting traversal.
- CPU parallelization.
- Full-precision LTC coefficient tables.
- Intermediate `.ltcbin` serialization.
- Checkpoint and resume.
- LUT-level numerical validation.

Before implementing Phase 4:

1. Inspect the existing repository.
2. Examine the actual Phase 3 intermediate format and data structures.
3. Review the reference LTC packing implementation.
4. Inspect available Unreal Engine GGX DDS files.
5. Identify the exact target texture layouts and shader decoding conventions.
6. Preserve all previous functionality.

Avoid unnecessary architectural changes.

---

## 2. Compatibility Targets

Support two explicitly separated export profiles.

### 2.1 Unreal Profile

Profile name:

```text
unreal
```

Target outputs:

```text
GGX_LTCMat.dds
GGX_LTCAmp.dds
```

The intended application is real-time GGX area-light evaluation.

The profile must reproduce the target Unreal Engine shader's expectations for:

- Texture resolution.
- Texture coordinate mapping.
- Matrix coefficient packing.
- Matrix normalization.
- Amplitude coefficient semantics.
- Pixel formats.
- Texel orientation.
- Texture filtering behavior.

The initial working assumptions are:

| Texture | Expected payload |
|---|---|
| GGX_LTCMat.dds | Four FP16 matrix coefficients |
| GGX_LTCAmp.dds | Two FP16 amplitude coefficients |

**These assumptions must be verified against the actual target Unreal Engine version before claiming full compatibility.**

Do not infer compatibility from filenames alone.

If the exact Unreal Engine texture contract cannot be verified, implement the documented provisional layout and report compatibility as unverified.

Do not falsely claim that the generated DDS files are interchangeable with Unreal Engine's original assets.

### 2.2 Reference Profile

Profile name:

```text
reference
```

Follow the published selfshadow LTC reference implementation.

Its matrix texture contains four coefficients, while its second texture contains:

- R: BRDF magnitude.
- G: Schlick Fresnel coefficient.
- B: Unused, set to zero.
- A: Spherical-cap integration coefficient.

The spherical-cap coefficient is independent of the GGX fitting result.

To support a complete reference-compatible export, implement the required auxiliary table generation if it is not already present.

Keep this implementation isolated from BRDF fitting.

The Reference profile uses two RGBA textures.

Do not incorrectly treat the reference's fourth channel as an LTC matrix or GGX amplitude coefficient.

### Compatibility Priority

Prioritize the Unreal profile as the primary project output.

Use the Reference profile as a mathematical and serialization compatibility baseline.

---

## 3. Architecture

Extend the project with independent packing and writing layers.

```text
              Phase 3
        Full-Precision LUTData
                  |
                  v
             LUTExporter
                  |
                  v
           LUTPackingProfile
                  |
          +-------+--------+
          |                |
          v                v
     Matrix Packing   Amplitude Packing
          |                |
          +-------+--------+
                  |
                  v
             TextureData
                  |
                  v
               DDSWriter
                  |
                  v
              DDS Files
                  |
                  v
               DDSReader
                  |
                  v
         Round-Trip Validation
```

### Required Separation

- LTC fitting must not depend on DDS serialization.
- LUT packing must not depend on CLI parsing.
- DDS Writer must not know anything about GGX or LTC.
- Texture packing must not run numerical optimization.
- Validation must use the existing DDS Reader wherever practical.
- Unreal-specific conventions must remain isolated inside the corresponding packing profile.

The DDS Writer must be reusable for future BRDF outputs.

---

## 4. Project Structure

Extend the project approximately as follows:

```text
include/ltc/
├── DDSWriter.h
├── TextureData.h
├── LUTPacking.h
├── LUTExporter.h
└── ExportConfig.h

src/
├── dds/
│   ├── DDSWriter.cpp
│   └── Float16.cpp
├── export/
│   ├── LUTPacking.cpp
│   ├── LUTExporter.cpp
│   ├── UnrealPacking.cpp
│   ├── ReferencePacking.cpp
│   └── ExportValidation.cpp
└── cli/
    └── main.cpp

tests/
├── Float16EncodingTests.cpp
├── DDSWriterTests.cpp
├── DDSRoundTripTests.cpp
├── LUTPackingTests.cpp
├── UnrealPackingTests.cpp
├── ReferencePackingTests.cpp
└── ExportIntegrationTests.cpp

docs/
├── dds-export.md
├── lut-packing.md
└── unreal-compatibility.md
```

Adapt the structure to existing repository conventions.

Do not create placeholder files without implementations.

---

## 5. LTC Matrix Packing

The LTC fitting engine produces a forward transformation matrix:

\[
M
\]

Real-time area-light evaluation typically uses its inverse:

\[
M^{-1}
\]

The published reference implementation normalizes the inverse matrix by its middle diagonal element.

Let:

\[
Q=M^{-1}
\]

Define:

\[
Q'=\frac{Q}{Q_{11}}
\]

where \(Q_{11}\) is the center diagonal element in zero-based mathematical matrix notation.

### Reference Packing

The reference stores four components.

Using the reference's GLM indexing:

```cpp
Mat3 invM = inverse(M);

invM /= invM[1][1];

texel.r = invM[0][0];
texel.g = invM[0][2];
texel.b = invM[2][0];
texel.a = invM[2][2];
```

**Important:** GLM indexing is column-first.

Do not confuse `matrix[column][row]` with conventional mathematical `matrix[row][column]`.

Depending on the internal matrix representation, the G and B channels may correspond to different mathematical row/column elements.

Write explicit tests to prove the mapping.

### Matrix Validation

Before packing:

- Validate all matrix entries.
- Validate invertibility.
- Validate the inverse matrix.
- Validate the normalization denominator.
- Reject zero or near-zero normalization denominators.
- Reject non-finite results.
- Check whether values are representable in the requested output format.

Do not silently replace invalid matrices with identity matrices.

Do not clamp signed matrix coefficients to `[0, 1]`.

Negative coefficients are valid.

### Matrix Packing Interface

Suggested structure:

```cpp
struct PackedLTCMatrix
{
    double r;
    double g;
    double b;
    double a;
};

PackedLTCMatrix packMatrix(
    const Mat3& matrix,
    LUTPackingProfile profile);
```

Use the existing matrix types where practical.

The profile-specific implementation must document the exact channel convention.

### Matrix Shape Assumptions

The four-component representation assumes a reduced isotropic LTC matrix structure.

Do not silently discard nonzero matrix components from an arbitrary custom BRDF.

If a matrix does not satisfy the required structural constraints within a documented tolerance, reject the export or explicitly report incompatibility.

Future anisotropic LTC fitting may require a different packing profile.

---

## 6. Amplitude Packing

Separate amplitude packing from matrix packing.

### 6.1 GGX Amplitude Terms

The Phase 2/3 fitting results contain:

- Magnitude.
- Schlick Fresnel coefficient.

For the Fresnel-free GGX target, the runtime may reconstruct the amplitude using:

\[
A(F_0)=F_0(A-F_c)+F_c
\]

where:

- \(A\) is the fitted magnitude.
- \(F_c\) is the integrated Schlick basis coefficient.
- \(F_0\) is the material's normal-incidence reflectance.

Do not confuse magnitude with the Fresnel coefficient.

Do not apply a fixed material `F0` during texture generation.

### 6.2 Unreal Amplitude Layout

The initial proposed layout is:

```text
R = Magnitude
G = FresnelCoefficient
```

Use an RG16F texture.

However, verify whether the target Unreal Engine runtime expects these raw coefficients or a transformed representation.

If Unreal Engine expects a different encoding, implement that encoding inside the Unreal packing profile.

Do not modify the mathematical fitting results to accommodate an output convention.

### 6.3 Reference Amplitude Layout

Use:

```text
R = Magnitude
G = FresnelCoefficient
B = 0
A = SphericalCapCoefficient
```

Output as RGBA16F.

The spherical-cap coefficient must be generated using the appropriate reference formula.

Do not copy amplitude values into the spherical-cap channel.

Do not fill the channel with fabricated data.

### Generic BRDF Behavior

The exporter must respect whether the BRDF provides a compatible Schlick Fresnel coefficient.

Do not silently emit zero for an unsupported Fresnel decomposition and label it as physically meaningful.

For a generic BRDF without compatible Fresnel terms, use an explicitly documented generic packing convention or reject the Unreal profile as incompatible.

---

## 7. Spherical-Cap Auxiliary Table

The selfshadow reference format includes a separate spherical-cap integration coefficient in the fourth channel of its second texture.

Implement this calculation only as required for the Reference export profile.

The implementation must:

- Follow the reference mathematical formula.
- Use the expected two-dimensional parameter mapping.
- Handle boundary cases.
- Avoid division by zero.
- Reject or report non-finite values.
- Provide independent numerical tests.

The table is not dependent on GGX roughness or BRDF fitting.

Keep it in a standalone helper module.

Do not generate it for the Unreal profile unless the target Unreal shader actually requires it.

The main objective remains the two Unreal-oriented DDS textures.

---

## 8. Floating-Point Encoding

Implement IEEE 754 binary16 encoding.

Phase 0 already includes FP16 decoding.

Phase 4 must add the inverse conversion:

```cpp
uint16_t float32ToFloat16(float value);
```

Use round-to-nearest, ties-to-even for the standard output mode.

Handle:

- Positive zero.
- Negative zero.
- Normal finite numbers.
- Subnormal numbers.
- Positive infinity.
- Negative infinity.
- NaN.
- Values exceeding FP16 finite range.
- Values smaller than the minimum FP16 subnormal.

### Important Policies

For generated LTC coefficients:

- Non-finite inputs must be reported as errors.
- Finite values that overflow FP16 must not silently become infinity.
- Negative values must remain signed.
- Small values must not be arbitrarily clamped.
- Quantization must be performed after LTC packing.
- The original full-precision LUTData must remain unchanged.

If strict replication of a legacy reference converter requires different rounding behavior, implement that explicitly in the Reference profile and document it.

Do not accidentally change the default IEEE-compliant conversion behavior.

### Required Tests

Use known bit patterns for:

```text
+0
-0
+1
-1
Maximum finite FP16
Minimum positive normal FP16
Minimum positive subnormal FP16
+Infinity
-Infinity
NaN
```

Also test:

- Values exactly halfway between representable FP16 values.
- Positive and negative rounding cases.
- Overflow boundaries.
- Underflow boundaries.
- Signed LTC matrix coefficients.

Compare against independently known expected encodings.

Do not use the same conversion implementation to generate the expected test data.

---

## 9. Texture Data Model

Introduce a generic intermediate texture representation.

Suggested types:

```cpp
enum class TextureFormat
{
    RG16F,
    RGBA16F,
    RG32F,
    RGBA32F
};

struct TextureData
{
    uint32_t width = 0;
    uint32_t height = 0;

    TextureFormat format;

    std::vector<std::byte> pixels;
};
```

### Requirements

- Explicit width and height.
- Explicit pixel format.
- Explicit channel count.
- Explicit byte layout.
- Contiguous pixel storage.
- No GPU API types.
- No Unreal Engine-specific types.
- Checked memory allocation sizes.

Use row-major image storage:

```cpp
pixelIndex = y * width + x;
```

For the LTC grid:

```text
x = roughnessIndex
y = viewIndex
```

Do not vertically flip or transpose the LUT during export unless the selected compatibility profile explicitly requires it.

The layout must be tested using synthetic, non-symmetric pixel patterns.

---

## 10. DDS Writer

Implement a standalone DDS Writer.

### Supported Formats

At minimum:

| Format | DXGI format | Bytes/pixel |
|---|---|---:|
| RG16F | R16G16_FLOAT | 4 |
| RGBA16F | R16G16B16A16_FLOAT | 8 |
| RG32F | R32G32_FLOAT | 8 |
| RGBA32F | R32G32B32A32_FLOAT | 16 |

Use the existing DDS Reader's format definitions where appropriate.

Avoid duplicating incompatible format enumerations.

### Default DDS Container

Use:

- DDS magic.
- Standard 124-byte DDS header.
- 20-byte DX10 extended header.
- Uncompressed floating-point pixel data.
- 2D texture resource.
- Array size 1.
- Mipmap count 1.
- No cubemap flags.
- No block compression.
- Little-endian encoding.

For an uncompressed image, populate the row pitch correctly.

Set header flags and resource properties in accordance with the DDS specification.

Do not assume that writing a valid DDS magic and pixel payload is sufficient.

### Legacy Header Compatibility

Phase 0 supports legacy DDS parsing.

If the Unreal reference assets use legacy FourCC floating-point formats, consider supporting an explicitly selected legacy output header.

Do not require legacy output unless the compatibility target needs it.

Do not equate file-header compatibility with numerical coefficient compatibility.

### Writer API

Suggested interface:

```cpp
class DDSWriter
{
public:
    static void write(
        const std::filesystem::path& path,
        const TextureData& texture);
};
```

Use the existing project's error-handling conventions.

### File Safety

- Validate dimensions and payload size.
- Detect size overflows.
- Reject unsupported formats.
- Detect file write failures.
- Avoid partially replacing existing valid outputs.
- Use temporary files where appropriate.
- Do not silently overwrite unrelated files.

DDS writing must not invoke BRDF fitting or LTC generation.

---

## 11. LUT Exporter

Implement a high-level LUT exporter that converts Phase 3 data into one or more DDS files.

Suggested interface:

```cpp
enum class ExportProfile
{
    Unreal,
    Reference
};

struct ExportConfig
{
    ExportProfile profile = ExportProfile::Unreal;

    TextureFormat matrixFormat =
        TextureFormat::RGBA16F;

    TextureFormat amplitudeFormat =
        TextureFormat::RG16F;

    std::filesystem::path outputDirectory;
};

class LUTExporter
{
public:
    void exportLUT(
        const LUTData& lut,
        const ExportConfig& config) const;
};
```

Adapt this to the actual repository.

### Export Pipeline

The exporter must:

1. Validate the LUT metadata.
2. Validate all cell completion states.
3. Validate fitted matrix data.
4. Select the packing profile.
5. Construct matrix texture data.
6. Construct amplitude texture data.
7. Quantize coefficients.
8. Write DDS files.
9. Read the files back using the DDS Reader.
10. Verify that the written data matches the packed representation.
11. Report output files and diagnostics.

### Invalid LUT Cells

Do not silently export a LUT containing invalid or incomplete fitting cells.

Default behavior must reject:

- Missing cells.
- Invalid matrices.
- Non-finite coefficients.
- Unsupported Fresnel models.
- Incompatible packing structures.

A cell that reached the fitting iteration limit but still contains a valid best-effort result may be accepted according to an explicit policy.

Report the number of such cells.

Do not automatically replace failed cells with neighboring values.

---

## 12. CLI Integration

Add a dedicated `export` command.

### Export Existing LUT

```bash
ltcgen export \
    --input ./ggx.ltcbin \
    --profile unreal \
    --output ./output
```

Expected outputs:

```text
output/
├── GGX_LTCMat.dds
└── GGX_LTCAmp.dds
```

### Reference Export

```bash
ltcgen export \
    --input ./ggx.ltcbin \
    --profile reference \
    --output ./reference-output
```

Use the reference profile's documented DDS filenames.

### High-Precision Export

Support FP32 output for debugging and numerical validation.

For example:

```bash
ltcgen export \
    --input ./ggx.ltcbin \
    --profile unreal \
    --precision fp32 \
    --output ./debug
```

Do not claim FP32 debug textures are interchangeable with Unreal Engine's original FP16 resources.

### Generate and Export

Optionally integrate DDS export into the existing generation command:

```bash
ltcgen generate \
    --brdf ggx \
    --width 64 \
    --height 64 \
    --profile reference \
    --threads 8 \
    --export-profile unreal \
    --output ./output
```

The fitting profile and export profile are independent:

- `--profile reference` controls numerical fitting.
- `--export-profile unreal` controls texture packing.

Do not use one ambiguous profile argument for both concepts.

### DDS Inspection

Preserve the existing inspection commands:

```bash
ltcgen inspect ./output/GGX_LTCMat.dds
```

```bash
ltcgen inspect ./output/GGX_LTCAmp.dds --stats
```

The generated DDS files must be readable by the Phase 0 DDS Reader.

---

## 13. Round-Trip Validation

Implement validation of the complete export pipeline.

### 13.1 Header Validation

After writing a DDS file, reopen it using the DDS Reader.

Verify:

- File magic.
- Header size.
- Resource dimension.
- Width and height.
- Array size.
- Mipmap count.
- Pixel format.
- Row pitch.
- Pixel data size.

Do not rely solely on the DDS Writer's internal state.

### 13.2 Pixel Validation

Compare decoded pixels against the expected packed coefficients.

For FP16:

1. Calculate the expected packed FP32 coefficient.
2. Encode it as FP16.
3. Read the written DDS pixel.
4. Verify that its decoded value matches the expected FP16 value.

For FP32:

- Compare the written and read values according to documented floating-point precision rules.

Do not compare FP16 decoded values directly against the original double-precision coefficients using zero tolerance.

### 13.3 Matrix Reconstruction

For every exported matrix texel, reconstruct the normalized inverse LTC matrix according to the selected profile.

Verify:

- Finite values.
- Correct component positions.
- Correct normalization.
- Correct sign.
- Expected matrix structure.

Compare the reconstructed matrix against the pre-quantization packed matrix.

Measure the quantization error.

### 13.4 Quantization Statistics

Report:

- Maximum absolute error.
- Mean absolute error.
- RMS error.
- Maximum relative error with a documented denominator floor.
- Number of overflowed coefficients.
- Number of non-finite coefficients.
- Number of failed cells.

Report matrix and amplitude statistics separately.

### 13.5 Lighting-Relevant Validation

A matrix coefficient can have a small absolute error while causing a larger change in the reconstructed LTC distribution.

For representative cells, compare:

- Full-precision LTC evaluation.
- LTC reconstructed from the packed DDS coefficients.

Evaluate several directions using the Phase 1 LTC mathematical core.

Account for the scale invariance introduced by inverse-matrix normalization.

Compare normalized directional distributions, not raw matrices with different overall scales.

This is a focused export-validation test, not a full area-light integration implementation.

---

## 14. Unreal Engine Compatibility Validation

The primary compatibility target is the existing Unreal Engine GGX LTC texture pair.

Reference files:

```text
GGX_LTCMat.dds
GGX_LTCAmp.dds
```

### 14.1 Reference Asset Inspection

When reference files are available:

1. Read their DDS metadata.
2. Determine dimensions.
3. Identify actual DXGI or legacy formats.
4. Identify channel counts.
5. Inspect coefficient ranges.
6. Check for NaN and infinity.
7. Compare expected shader packing.
8. Record findings.

Do not assume that the reference textures are exactly 64×64 without inspection.

Do not assume the Unreal DDS headers match those emitted by the published selfshadow reference.

### 14.2 Runtime Contract

Locate and inspect the relevant Unreal Engine shader code for the specific engine version being targeted.

Verify:

- Matrix coefficient ordering.
- Amplitude channel semantics.
- Texture coordinate transformation.
- Roughness parameterization.
- View-angle parameterization.
- Texture filtering.
- Matrix reconstruction.

A valid DDS file is not automatically a compatible LTC texture.

Document the target Unreal Engine version and relevant shader/source locations.

### 14.3 Numerical Comparison

Compare generated textures with the available Unreal reference textures.

Report:

- Dimensions and formats.
- Per-channel maximum error.
- Per-channel mean error.
- Per-channel RMS error.
- Coefficient distributions.
- Largest-error pixel locations.

Distinguish:

1. Differences from numerical fitting.
2. Differences from packing conventions.
3. Differences from FP16 conversion.
4. Differences from DDS serialization.

Do not attribute all discrepancies to fitting without investigating the packing pipeline.

### 14.4 Missing Reference Assets

If Unreal reference DDS files or source code are unavailable:

- Do not fabricate reference values.
- Do not claim full compatibility.
- Run synthetic and selfshadow-reference validation instead.
- Document which compatibility checks remain unverified.
- Preserve the ability to perform the Unreal comparison later.

Do not introduce an Unreal Engine dependency into the build.

---

## 15. Generic BRDF Export

The LUT Generator is intended to support multiple BRDFs.

Do not hardcode GGX into the generic DDS Writer.

Use a data-driven export descriptor or explicit packing profile.

For example:

```text
BRDF: GGX
Matrix texture: GGX_LTCMat.dds
Amplitude texture: GGX_LTCAmp.dds
Packing: Unreal
```

Future generic output may use names such as:

```text
Beckmann_LTCMat.dds
Beckmann_LTCAmp.dds
```

However, adding a new BRDF must not imply Unreal Engine compatibility.

The export profile must validate that the mathematical data supports its required representation.

Do not build a dynamic plugin system in Phase 4.

---

## 16. Automated Tests

Extend the existing GoogleTest suite.

### FP16 Tests

Verify:

- Known conversions.
- Correct rounding.
- Signed zero.
- Subnormal values.
- Overflow detection.
- NaN and infinity handling.
- Signed coefficients.

### DDS Writer Tests

Use synthetic textures with known values.

Verify:

- Valid DDS headers.
- Correct DXGI format.
- Correct image dimensions.
- Correct row pitch.
- Correct payload size.
- Correct channel order.
- Correct little-endian serialization.
- Correct error handling.

### Matrix Packing Tests

Use known invertible matrices, including matrices with distinct off-diagonal terms.

Verify:

- Correct inverse.
- Correct center-diagonal normalization.
- Correct four-component extraction.
- Correct column-major versus row-major interpretation.
- Signed coefficient preservation.
- Invalid matrix rejection.

### Amplitude Packing Tests

Verify:

- Magnitude channel.
- Fresnel channel.
- Unsupported Fresnel behavior.
- Reference spherical-cap channel.
- Correct texture format.

### DDS Round-Trip Tests

Write and reload:

- RG16F.
- RGBA16F.
- RG32F.
- RGBA32F.

Compare every pixel with independently expected values.

### LUT Export Tests

Create a small synthetic LUTData object.

Verify:

- Correct output filenames.
- Correct pixel mapping.
- Correct matrix coefficients.
- Correct amplitude coefficients.
- Correct rejection of invalid cells.
- Correct output profile selection.

### Full Integration Test

Load a completed Phase 3 `.ltcbin` file if a suitable test fixture is available.

Export DDS textures.

Reopen them with the DDS Reader.

Validate the full payload.

Do not require a complete 64×64 fitting run during ordinary unit tests.

### External Compatibility Tests

When Unreal Engine reference textures are available, run numerical comparisons as an optional test suite.

Do not make proprietary Unreal Engine assets mandatory for standard CI.

---

## 17. Documentation

Update the project documentation.

### docs/dds-export.md

Document:

- Supported DDS formats.
- DDS header structure.
- DX10 extension.
- Legacy header handling.
- FP16 encoding.
- Pixel memory layout.
- File writing behavior.
- Error handling.

### docs/lut-packing.md

Document:

- Matrix inversion.
- Matrix normalization.
- Four-channel packing.
- Amplitude semantics.
- Reference auxiliary table.
- Row/column indexing.
- Texture coordinate mapping.
- Quantization.

Include numerical examples with independently verified results.

### docs/unreal-compatibility.md

Document:

- Target Unreal Engine version.
- Required texture filenames.
- Actual texture formats.
- Shader decoding conventions.
- Reference asset inspection results.
- Verified compatibility.
- Unverified assumptions.
- Known differences from the published LTC reference.

### README.md

Add examples for:

- Exporting an existing LUT.
- Generating and exporting a LUT.
- Inspecting generated DDS files.
- Comparing against reference textures.

Clearly distinguish between supported and experimentally compatible profiles.

---

## 18. Scope Restrictions

Phase 4 must not implement:

- New BRDF models.
- New fitting algorithms.
- Alternative numerical optimizers.
- GPU-accelerated fitting.
- Full area-light polygon integration.
- Unreal Engine runtime modifications.
- Unreal Engine plugins.
- GUI.
- Path-tracing-specific LTC features.
- Sheen importance-sampling LUT generation.
- EXR or PNG texture export.
- Compressed DDS formats.
- Mipmapped LUT generation.
- Arbitrary anisotropic matrix packing.

Keep the scope focused on DDS output and its numerical correctness.

---

## 19. Acceptance Criteria

Phase 4 is complete when:

- [ ] The existing Phase 3 LUT format can be loaded.
- [ ] The LTC matrix packing layer is implemented.
- [ ] Inverse-matrix normalization is correct.
- [ ] Matrix component ordering is documented and tested.
- [ ] GGX magnitude and Fresnel packing works.
- [ ] IEEE 754 FP16 encoding is implemented and tested.
- [ ] RG16F and RGBA16F DDS output works.
- [ ] RG32F and RGBA32F debug output works.
- [ ] DDS headers and pixel payloads are valid.
- [ ] Generated textures can be read by the existing DDS Reader.
- [ ] DDS round-trip tests pass.
- [ ] Unreal-oriented DDS filenames are generated.
- [ ] Reference export packing is implemented.
- [ ] Reference auxiliary data is generated where required.
- [ ] Invalid LUT data is rejected.
- [ ] Quantization errors are measured.
- [ ] Matrix reconstruction tests pass.
- [ ] CLI export works without rerunning fitting.
- [ ] Generate-and-export workflow works.
- [ ] Unreal compatibility checks are performed when the relevant assets and shader sources are available.
- [ ] Any unverified compatibility assumptions are documented.
- [ ] All Phase 0–3 regression tests pass.
- [ ] The project builds without Unreal Engine or a GPU SDK.
- [ ] Documentation is updated.

**Full Unreal Engine compatibility must only be claimed after its actual texture and shader conventions have been verified.**

---

## 20. Implementation Procedure

Implement Phase 4 in the following order:

1. Inspect the existing repository and Phase 3 LUT representation.
2. Review the selfshadow reference packing and DDS writer.
3. Inspect available Unreal Engine DDS assets and shader conventions.
4. Document the exact packing profiles.
5. Implement the generic TextureData model.
6. Implement and test FP16 encoding.
7. Implement the DDS Writer.
8. Add DDS Writer and round-trip tests.
9. Implement reference inverse-matrix packing.
10. Add matrix packing tests.
11. Implement amplitude packing.
12. Add amplitude packing tests.
13. Implement spherical-cap auxiliary data for the Reference profile.
14. Implement the Unreal packing profile.
15. Implement the LUT Exporter.
16. Integrate the `ltcgen export` CLI command.
17. Integrate optional export after LUT generation.
18. Implement post-export validation.
19. Compare generated DDS textures with reference assets where available.
20. Run all Phase 0–4 tests.
21. Update documentation.
22. Produce the implementation report.

Prefer incremental changes.

Validate each serialization and packing component independently before integrating the full export pipeline.

Do not change numerical fitting merely to force the output coefficients to match an unrelated reference.

### Final Report

After implementation, provide:

1. Summary of implemented functionality.
2. Files created and modified.
3. Supported DDS formats.
4. Matrix packing and normalization conventions.
5. Amplitude packing conventions.
6. FP16 conversion and rounding policy.
7. Generated output filenames and sizes.
8. DDS round-trip validation results.
9. Quantization error statistics.
10. Unreal Engine reference comparison results, if available.
11. Unverified compatibility details.
12. Build and test results.
13. Known limitations.
14. Deviations from the specification.
15. Readiness for Phase 5.

**Expected result: A fully functional DDS export pipeline that converts Phase 3 LTC coefficient tables into GPU-ready GGX LTC textures, with verified matrix packing, FP16 encoding, DDS serialization, round-trip correctness, and clearly documented Unreal Engine compatibility.**
