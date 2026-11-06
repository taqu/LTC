# Phase 0 — Project Foundation and DDS Inspection

## Project: LTC LUT Generator

### Objective

Establish the foundation for a standalone, CPU-based LTC (Linearly Transformed Cosines) LUT generation tool.

The tool will eventually fit LTC distributions to multiple BRDF models and generate DDS textures for real-time area-light evaluation.

The initial reference target is Unreal Engine's GGX LTC textures:

- `GGX_LTCMat.dds`
- `GGX_LTCAmp.dds`

Phase 0 must focus strictly on project infrastructure, command-line functionality, DDS inspection, and automated tests.

**Do not implement BRDF evaluation, LTC fitting, numerical optimization, or LUT generation in this phase.**

---

## 1. Technology Requirements

Use the following technologies:

| Component | Requirement |
|---|---|
| Language | C++20 |
| Build system | CMake |
| Platform | Windows, Linux, macOS |
| Primary development platform | Windows |
| Execution | Standalone CLI |
| GPU dependencies | None |
| Test framework | GoogleTest |
| CLI parser | CLI11 |
| Configuration | nlohmann/json |
| Image format | DDS |
| Numerical computation | CPU only |

### General Requirements

- Use modern C++20.
- Enable strict compiler warnings.
- Avoid platform-specific code wherever possible.
- Support MSVC, Clang, and GCC.
- Avoid requiring a graphics API or GPU runtime.
- Keep third-party dependencies minimal.
- Support reproducible builds.
- Use RAII for resource management.
- Prefer standard C++ types and containers.
- Use `std::filesystem` for path handling.
- Ensure all binary file operations are performed safely.
- Keep the architecture suitable for future multithreaded CPU fitting.

Do not introduce unnecessary abstractions or plugin frameworks.

---

## 2. Project Structure

Create the following initial directory structure:

```text
LTCGenerator/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
├── .gitignore
├── cmake/
├── include/
│   └── ltc/
│       ├── Version.h
│       ├── DDS.h
│       └── DDSReader.h
├── src/
│   ├── core/
│   │   └── Version.cpp
│   ├── dds/
│   │   └── DDSReader.cpp
│   └── cli/
│       └── main.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── DDSReaderTests.cpp
│   └── CLITests.cpp
├── reference/
│   └── README.md
├── examples/
│   └── README.md
└── docs/
    ├── architecture.md
    ├── dds-format.md
    └── development.md
```

Adjust the structure if necessary to follow established repository conventions.

Do not create placeholder source files that serve no purpose.

### Architectural Boundaries

Organize the project so that:

- The DDS reader can be used independently of the CLI.
- The CLI does not contain DDS parsing logic.
- Future BRDF implementations will not depend on DDS internals.
- Future fitting components will not depend on output file formats.
- Validation components can use the DDS reader without invoking CLI commands.

The DDS reader must be a reusable library component rather than an implementation embedded in `main.cpp`.

---

## 3. CMake Build System

Implement a clean CMake-based build system.

### Requirements

- Require C++20.
- Use target-specific compiler options.
- Avoid modifying global compiler flags unnecessarily.
- Support Debug and Release configurations.
- Support MSVC, GCC, and Clang.
- Export `compile_commands.json` where supported.
- Use CTest for automated tests.
- Make tests optional through a CMake option.
- Do not require a GPU SDK.
- Do not require Unreal Engine to build.

Use an option such as:

```cmake
option(LTC_BUILD_TESTS "Build LTC Generator tests" ON)
```

### Build Targets

Create at least:

```text
ltc_core
ltcgen
ltc_tests
```

Responsibilities:

- `ltc_core`: reusable DDS inspection and foundational functionality.
- `ltcgen`: command-line executable.
- `ltc_tests`: automated tests.

Prefer linking `ltcgen` against `ltc_core`.

### Dependencies

Use CMake FetchContent or another clearly documented CMake-native dependency management approach.

Required libraries:

- CLI11
- nlohmann/json
- GoogleTest (tests only)

Do not introduce a large graphics framework.

Pin dependency versions for reproducibility.

---

## 4. Command-Line Interface

Implement the initial CLI executable:

```bash
ltcgen
```

### Required Commands

#### Help

```bash
ltcgen --help
```

Display available commands and their descriptions.

#### Version

```bash
ltcgen --version
```

Print the tool version.

#### DDS Inspection

```bash
ltcgen inspect GGX_LTCMat.dds
```

Inspect the supplied DDS file and print its metadata.

Example output:

```text
File: GGX_LTCMat.dds
Container: DDS
Width: 64
Height: 64
Depth: 1
Mip levels: 1
Array size: 1
Format: DXGI_FORMAT_R16G16B16A16_FLOAT
Channels: 4
Bytes per pixel: 8
Data size: 32768 bytes
```

The displayed values must come from the actual DDS file. The values above are illustrative.

#### JSON Inspection

Support machine-readable output:

```bash
ltcgen inspect GGX_LTCMat.dds --json
```

Example:

```json
{
  "file": "GGX_LTCMat.dds",
  "container": "DDS",
  "width": 64,
  "height": 64,
  "depth": 1,
  "mipLevels": 1,
  "arraySize": 1,
  "format": "R16G16B16A16_FLOAT",
  "channels": 4,
  "bytesPerPixel": 8,
  "dataSize": 32768
}
```

The JSON structure should be stable and documented.

#### Pixel Inspection

Support inspecting an individual pixel:

```bash
ltcgen inspect GGX_LTCMat.dds --pixel 12,24
```

Example output:

```text
Pixel (12, 24):
  R: 1.023438
  G: 0.000000
  B: -0.125000
  A: 0.984375
```

These values are illustrative.

The command must decode the actual floating-point pixel data.

Use zero-based coordinates, with (0, 0) referring to the first pixel in the DDS payload. Document the image orientation explicitly.

Support pixel inspection in JSON mode as well.

### CLI Error Handling

Handle:

- Missing input files.
- Invalid DDS files.
- Unsupported formats.
- Invalid command-line arguments.
- Out-of-range pixel coordinates.
- Truncated files.
- File access errors.

Errors must produce useful diagnostic messages and nonzero exit codes.

Normal command output must not be mixed with diagnostic messages in JSON mode.

---

## 5. DDS Reader

Implement a standalone DDS reader capable of inspecting the existing Unreal Engine LTC textures.

### Supported Formats

At minimum, implement:

- `DXGI_FORMAT_R16G16_FLOAT`
- `DXGI_FORMAT_R16G16B16A16_FLOAT`
- `DXGI_FORMAT_R32G32_FLOAT`
- `DXGI_FORMAT_R32G32B32A32_FLOAT`

The primary focus is FP16 and FP32 data.

Additional formats may be implemented if required by the supplied reference textures.

### DDS Header Support

Support:

1. Legacy DDS header.
2. DDS DX10 extended header.

Parse and validate:

- DDS magic.
- Header sizes.
- Image dimensions.
- Pixel format.
- DX10 extension presence.
- Resource dimension.
- Array size.
- Mipmap count.
- Data offset.
- Pixel payload size.

The initial reader only needs to decode uncompressed floating-point 2D textures.

Unsupported resource types must be rejected explicitly.

Do not silently interpret unsupported data as a known format.

### Robustness Requirements

- Use fixed-width integer types.
- Avoid unsafe pointer reinterpretation of untrusted file data.
- Validate all sizes and offsets before reading.
- Detect integer overflow in size calculations.
- Validate file bounds.
- Reject malformed or inconsistent headers.
- Do not assume the file is valid merely because the DDS magic matches.
- Never read beyond the end of a file.
- Avoid unnecessary copying of large payloads.

The reader should support loading metadata independently from loading the full pixel data where practical.

### Floating-Point Decoding

Implement correct IEEE 754 binary16-to-binary32 conversion.

Handle:

- Positive and negative zero.
- Normalized values.
- Subnormal values.
- Positive and negative infinity.
- NaN.

Use C++20-compatible code without relying on C++23 `std::float16_t`.

Preserve the sign of negative zero when feasible.

Do not automatically clamp HDR or signed coefficients.

Negative matrix coefficients are valid data.

### Public API

Design a reusable API similar to:

```cpp
namespace ltc {

enum class DDSFormat {
    Unknown,
    RG16F,
    RGBA16F,
    RG32F,
    RGBA32F
};

struct DDSMetadata {
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t mipLevels = 1;
    uint32_t arraySize = 1;

    DDSFormat format = DDSFormat::Unknown;

    uint32_t channelCount = 0;
    uint32_t bytesPerPixel = 0;

    uint64_t dataOffset = 0;
    uint64_t dataSize = 0;
};

struct Pixel {
    std::array<float, 4> channels{};
    uint32_t channelCount = 0;
};

class DDSReader {
public:
    static DDSMetadata readMetadata(
        const std::filesystem::path& path);

    static Pixel readPixel(
        const std::filesystem::path& path,
        uint32_t x,
        uint32_t y);
};

}
```

This API is illustrative, not mandatory.

Feel free to improve error handling using explicit result types or well-defined exceptions.

The final implementation must document its error-handling policy.

For Phase 0, support pixel access to the first mip level of a non-array 2D texture.

Metadata inspection may report additional mip levels or array sizes, but attempting to read an unsupported subresource must produce an explicit error.

### Legacy Floating-Point DDS Formats

Some existing DDS files use legacy floating-point FourCC formats rather than the DX10 extended header.

Inspect the reference textures if they are available.

Add legacy format support if necessary to read the target GGX textures.

Do not assume that all Unreal Engine LTC textures use the DX10 header.

---

## 6. Unreal Engine Reference Textures

The primary reference files are:

```text
GGX_LTCMat.dds
GGX_LTCAmp.dds
```

These files are intended for real-time area-light LTC evaluation.

`Sheen_LTC.dds` is not part of the initial scope because the identified Unreal Engine usage concerns LTC-based importance sampling for path tracing.

### Reference Data Handling

Do not assume that Unreal Engine reference files are present in the repository.

Use a configurable reference directory.

For example:

```text
reference/
    README.md
    unreal/
        GGX_LTCMat.dds
        GGX_LTCAmp.dds
```

If reference files are available:

- Inspect their headers.
- Identify exact dimensions.
- Identify DDS format and header variant.
- Verify that their pixels can be decoded.
- Report channel ranges.
- Check for NaN and infinity.
- Record the findings in documentation.

If they are unavailable:

- Do not invent their metadata.
- Do not download unrelated substitute textures.
- Continue implementation using synthetic DDS fixtures.
- Document how users can supply the reference files.

Do not commit proprietary or redistributable-restricted assets without verifying their licensing terms.

---

## 7. DDS Statistics

Add an optional statistics mode:

```bash
ltcgen inspect GGX_LTCMat.dds --stats
```

Report per-channel statistics:

- Minimum.
- Maximum.
- Mean.
- Number of NaN values.
- Number of positive infinities.
- Number of negative infinities.

Example:

```text
Channel statistics:

         Min       Max       Mean
R        ...       ...       ...
G        ...       ...       ...
B        ...       ...       ...
A        ...       ...       ...

Non-finite values:
NaN:  0
+Inf: 0
-Inf: 0
```

Compute statistics using double-precision accumulators.

Statistics must use decoded floating-point values rather than raw FP16 bit patterns.

Ignore non-finite values when computing min, max, and mean, but report their counts separately.

If a channel contains no finite values, report its finite statistics as unavailable.

Ensure JSON output can represent these cases without emitting invalid JSON numeric literals.

---

## 8. Automated Tests

Use GoogleTest and CTest.

### DDS Header Tests

Test:

- Valid legacy DDS.
- Valid DX10 DDS.
- Invalid DDS magic.
- Truncated header.
- Incorrect header size.
- Missing DX10 extension.
- Invalid dimensions.
- Unsupported resource dimension.
- Unsupported compressed format.
- Unsupported or inconsistent pixel format.
- Truncated pixel payload.
- Integer overflow in size calculations.

### FP16 Conversion Tests

Verify:

- `0.0`
- `-0.0`
- `1.0`
- `-1.0`
- Maximum finite FP16.
- Minimum positive normal FP16.
- Minimum positive subnormal FP16.
- Positive and negative infinity.
- NaN.

Use known IEEE 754 bit patterns.

### Pixel Reading Tests

Generate small synthetic DDS fixtures containing known FP16/FP32 pixel values.

Test:

- Correct coordinate mapping.
- Correct channel ordering.
- Correct signed values.
- Correct pixel stride.
- Boundary coordinates.
- Invalid coordinates.
- Truncated payloads.

Keep these tests independent from Unreal Engine assets.

### CLI Tests

Test:

- `--help`
- `--version`
- `inspect`
- `inspect --json`
- `inspect --pixel`
- `inspect --stats`
- Invalid input paths.
- Invalid arguments.
- Unsupported DDS formats.

Verify exit codes and machine-readable output where appropriate.

### Test Quality

Tests must not merely verify that functions return without crashing.

They must compare decoded metadata and floating-point values against known expectations.

---

## 9. Documentation

Create or update the following documents.

### README.md

Include:

- Project overview.
- Supported platforms.
- Build requirements.
- Build instructions.
- Basic CLI usage.
- DDS inspection examples.
- Current limitations.
- Development roadmap.

### docs/architecture.md

Describe:

- Main components.
- Dependencies between components.
- Architectural boundaries.
- Planned BRDF fitting integration.
- Planned DDS output architecture.

Clearly distinguish implemented Phase 0 functionality from future functionality.

### docs/dds-format.md

Document:

- DDS legacy header.
- DDS DX10 header.
- Supported DXGI formats.
- Legacy floating-point FourCC formats where implemented.
- Pixel layout.
- Channel ordering.
- Row pitch assumptions.
- FP16 decoding.
- Unsupported formats and resource types.

### docs/development.md

Document:

- CMake build steps.
- Debug and Release builds.
- Running tests.
- Compiler requirements.
- Dependency management.
- How to add tests.
- How to use local Unreal Engine reference DDS files.

### Licensing

Respect the repository's existing license if one exists.

If the repository has no license, do not assume that the original LTC reference implementation's license automatically applies to this project.

Document third-party licenses and attribution requirements.

Do not copy reference implementation source code without reviewing its licensing terms.

---

## 10. Build and Verification

Provide documented commands equivalent to:

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DLTC_BUILD_TESTS=ON

cmake --build build --config Release

ctest --test-dir build \
    -C Release \
    --output-on-failure
```

Ensure that the project works without Unreal Engine installed.

Where the corresponding build environments are available, test with:

- Windows + MSVC.
- Linux + GCC.
- Linux/macOS + Clang.

Do not claim a platform has been tested unless the corresponding build and tests were actually executed.

---

## 11. Scope Restrictions

Phase 0 must **not** implement:

- GGX BRDF evaluation.
- Beckmann BRDF evaluation.
- Disney BRDF evaluation.
- LTC distribution evaluation.
- LTC importance sampling.
- Nelder–Mead optimization.
- BRDF fitting.
- LUT generation.
- DDS writing.
- GPU acceleration.
- GUI.
- Unreal Engine runtime integration.
- Path-tracing-specific LTC support.

These belong to later phases.

Focus on a reliable, maintainable foundation.

---

## 12. Acceptance Criteria

Phase 0 is complete when all of the following conditions are satisfied:

- [ ] The project builds with C++20 and CMake.
- [ ] The CLI executable runs and provides help/version information.
- [ ] DDS metadata can be inspected.
- [ ] Both legacy and DX10 DDS headers are correctly recognized.
- [ ] Supported FP16 and FP32 textures can be decoded.
- [ ] Individual DDS pixels can be inspected.
- [ ] Channel statistics can be calculated.
- [ ] JSON inspection output works.
- [ ] Invalid and truncated DDS files are handled safely.
- [ ] Automated tests cover headers, pixels, FP16 decoding, and CLI behavior.
- [ ] CTest executes the test suite successfully.
- [ ] The DDS Reader is independent of the CLI.
- [ ] Documentation accurately reflects the implementation.
- [ ] The project builds without a GPU SDK or Unreal Engine.
- [ ] The implementation does not contain premature BRDF or LTC fitting functionality.

---

## 13. Implementation Instructions

Before making changes:

1. Inspect the existing repository, if any.
2. Identify existing build conventions, coding standards, and dependencies.
3. Preserve useful existing infrastructure.
4. Inspect available Unreal Engine reference DDS files without assuming their format.

Then implement Phase 0 incrementally.

Prioritize correctness, testability, and clear error handling over unnecessary complexity.

Do not implement future phases early.

Do not leave placeholder implementations that pretend to support functionality that is not yet available.

If a required dependency or reference asset is unavailable, handle the limitation explicitly rather than silently substituting unrelated behavior.

### Final Report

After implementation, provide:

1. A summary of the implemented features.
2. The resulting project structure.
3. The supported DDS formats.
4. The actual metadata of any reference DDS files inspected.
5. Build and test commands.
6. Test results, including failed or skipped tests.
7. Known limitations.
8. Any deviations from this specification and their rationale.
9. Readiness for Phase 1.

**The expected result is a working C++20 CLI application with a robust, reusable DDS Reader and verified test infrastructure—not an LTC fitting implementation.**
