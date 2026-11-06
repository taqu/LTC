# Task — Reorganize README and Improve User Documentation

## Project: LTC LUT Generator

### Objective

Review and reorganize the project's `README.md` to make it clear, concise, accurate, and useful for developers who want to generate LTC LUT textures.

The README should serve as the primary entry point for users who want to:

1. Understand what the tool does.
2. Build it from source.
3. Generate GGX LTC LUTs.
4. Export DDS textures.
5. Generate LUTs for other supported BRDFs.
6. Validate generated textures.
7. Find advanced configuration and technical documentation.

The project is a C++20, CPU-based, command-line LTC fitting and LUT generation tool intended primarily for real-time area-light rendering.

**The main goal is to improve documentation quality, not to change or implement application functionality.**

---

## 1. Inspect the Existing Repository

Before editing documentation:

1. Read the current `README.md`.
2. Inspect the actual CLI implementation and help output.
3. Review the CMake configuration and presets.
4. Identify supported operating systems and compiler requirements.
5. Inspect the existing `docs/` directory.
6. Identify implemented BRDF models.
7. Verify the supported DDS output formats.
8. Verify the actual generated filenames.
9. Review the current reference and quality fitting profiles.
10. Identify any existing examples and validation commands.

Do not assume that all features described in the development roadmap are implemented.

The repository source code and actual CLI behavior are the authoritative sources of truth.

If documentation and implementation disagree, correct the documentation rather than inventing commands or changing application behavior.

---

## 2. README Design Principles

The README must be:

- Written in clear, natural English.
- Concise but sufficiently informative.
- Suitable for graphics programmers and rendering engineers.
- Focused on practical usage.
- Easy to scan.
- Accurate to the current implementation.
- Free from unnecessary marketing language.
- Consistent with the existing CLI terminology.
- Organized from basic usage to advanced functionality.

Avoid turning the README into a mathematical paper or internal design document.

Prefer short explanations and working examples.

Do not duplicate detailed documentation already maintained under `docs/`.

Use relative links to repository documentation.

Do not include excessive badges, decorative icons, or large screenshots without a clear purpose.

---

## 3. Recommended README Structure

Reorganize the README using approximately the following structure:

```text
# LTC LUT Generator

Brief project description

## Features

## Requirements

## Quick Start

## Generating LTC LUTs

## Supported BRDFs

## DDS Output

## Validation

## Advanced Usage

## Documentation

## References

## License
```

Adjust section names when necessary.

Avoid excessive nesting or fragmentation.

The Quick Start should appear near the beginning, not after lengthy technical explanations.

---

## 4. Project Introduction

Begin with a concise explanation of the project's purpose.

Suggested wording:

"LTC LUT Generator is a standalone C++20 command-line tool for fitting Linearly Transformed Cosines to BRDF distributions and generating DDS lookup textures for real-time area-light rendering."

Explain briefly that:

- Fitting runs on the CPU.
- The tool supports multiple BRDF models.
- It generates reusable GPU lookup textures.
- Numerical validation is available.
- Unreal Engine GGX LTC conventions are an important compatibility target.

Do not imply that the tool is an Unreal Engine plugin.

Do not claim compatibility with every Unreal Engine version.

Keep the introduction to one or two short paragraphs.

---

## 5. Features

Present the main implemented capabilities in a compact list.

Potential features include:

- CPU-based LTC fitting.
- GGX, Beckmann, and Disney Diffuse support.
- Custom BRDF extension.
- Reference and Quality fitting profiles.
- Multithreaded LUT generation.
- DDS texture export.
- Checkpoint and resume.
- Numerical validation.
- Reference texture comparison.
- Area-light approximation validation.
- Benchmarking and fitting diagnostics.

Verify every feature against the actual implementation.

Remove features that are incomplete or unavailable.

Do not include a detailed phase-by-phase development history in the main feature list.

---

## 6. Requirements and Building

Document the actual build requirements.

Expected technologies include:

- C++20 compiler.
- CMake.
- Supported Windows, Linux, and macOS toolchains.
- Project-specific dependency requirements.

Verify supported compiler versions rather than guessing.

### Building from Source

Provide copy-pasteable commands using the project's actual build system.

For example:

```bash
cmake -S . -B build -DLTC_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

These commands are illustrative.

Use the actual CMake options, targets, and presets.

If the project uses `CMakePresets.json`, consider documenting the recommended preset-based workflow.

Explain where the resulting executable is located.

Account for multi-configuration generators such as Visual Studio.

Do not assume that the executable appears in the same directory on all platforms.

Keep advanced build customization in `docs/development.md`.

---

## 7. Quick Start

The Quick Start must demonstrate the shortest practical workflow from source code to usable DDS textures.

Ideally, a reader should be able to:

1. Build the executable.
2. Generate a GGX LUT.
3. Export it as DDS.
4. Inspect the output.

Use commands verified against the current CLI.

An illustrative workflow is:

```bash
ltcgen generate \
    --brdf ggx \
    --width 64 \
    --height 64 \
    --profile reference \
    --output ggx.ltcbin
```

```bash
ltcgen export \
    --input ggx.ltcbin \
    --profile unreal \
    --output ./output
```

Expected DDS files:

```text
output/
├── GGX_LTCMat.dds
└── GGX_LTCAmp.dds
```

The examples must use the actual CLI syntax and filenames.

Do not present these commands as verified if the implementation differs.

If a single command can generate and export textures, prefer the simpler workflow for Quick Start.

Document the separate generation and export workflow under Advanced Usage.

### Platform Considerations

Use a command style that is easy to understand.

If shell syntax differs between Windows PowerShell and Unix shells, either provide clearly labeled variants or use commands that work without multiline continuation.

Avoid assuming that Unix-style backslash continuation works in PowerShell.

---

## 8. Generating LTC LUTs

Explain the primary generation command.

Cover:

- BRDF selection.
- LUT resolution.
- Fitting profile.
- CPU thread count.
- Output location.

Provide a standard GGX example.

Explain the difference between:

**Reference Mode**

Designed to reproduce the established LTC reference fitting conventions.

**Quality Mode**

Uses additional optimization and validation strategies to improve fitting quality.

Clarify that Quality Mode may require substantially more CPU time.

Do not claim that Quality Mode always produces better lighting results.

### Checkpoint and Resume

If implemented, provide short examples showing:

- Starting a checkpointed generation.
- Resuming an interrupted generation.

Explain that checkpoints are intermediate fitting state, not DDS textures.

Link to advanced documentation for details.

---

## 9. Supported BRDFs

Document the BRDF implementations actually available.

For example:

| BRDF | Description |
|---|---|
| GGX | Isotropic microfacet reflection |
| Beckmann | Isotropic Beckmann microfacet reflection |
| Disney Diffuse | Burley diffuse reflection |

Explain briefly that each BRDF may have different roughness parameter semantics.

Do not imply that the same numerical roughness parameter has an identical physical meaning across all BRDF models.

Provide one short example of generating a non-GGX LUT.

Link to the BRDF extension documentation for users who want to implement custom BRDFs.

Do not include complete BRDF equations in the README.

---

## 10. DDS Output

Describe the generated texture files and their intended use.

For the Unreal-oriented GGX profile, document:

| Output | Purpose |
|---|---|
| `GGX_LTCMat.dds` | Packed LTC inverse transformation matrix |
| `GGX_LTCAmp.dds` | LTC amplitude coefficients |

Verify actual file formats and channel layouts before documenting them.

Explain that the Matrix and Amplitude textures must be interpreted using the corresponding runtime packing convention.

Include a short note that:

- DDS output contains floating-point coefficients.
- The textures are intended for use in rendering shaders.
- Texture coordinate mapping and matrix reconstruction must match the selected packing profile.

### Unreal Engine Compatibility

Distinguish between:

- Unreal-oriented output naming.
- Matching Unreal Engine texture packing conventions.
- Verified compatibility with a specific Unreal Engine version.

Do not claim full Unreal Engine compatibility unless it has been demonstrated.

If compatibility is experimental or partially verified, state that clearly.

Do not imply that every generated BRDF texture can replace Unreal Engine's GGX textures.

Link to `docs/unreal-compatibility.md` if available.

---

## 11. Validation

Explain how users can validate generated LUTs.

Keep the README focused on practical commands.

Include examples for the implemented validation operations, such as:

- DDS structure inspection.
- Reference DDS comparison.
- LTC distribution validation.
- Area-light numerical validation.

For example:

```bash
ltcgen inspect ./output/GGX_LTCMat.dds
```

If supported, show one representative validation command that generates a JSON report.

Explain briefly that numerical validation can measure:

- Coefficient differences.
- LTC approximation error.
- FP16 quantization error.
- Area-light integration error.

Do not describe detailed integration algorithms in the README.

Link to `docs/validation.md` or the appropriate existing document.

Clearly distinguish reference compatibility from actual lighting approximation quality.

---

## 12. Advanced Usage

Keep advanced topics brief.

Potential topics include:

- Custom BRDF implementations.
- Quality fitting configuration.
- Multithreaded generation.
- Checkpoint and resume.
- LUT refinement.
- Benchmark commands.
- Intermediate LUT formats.

Provide links to detailed documentation instead of repeating entire explanations.

Do not create an Advanced Usage section containing dozens of unrelated CLI examples.

---

## 13. Documentation Index

Add a compact documentation index linking to existing documents.

Organize documentation by user task.

Suggested categories:

| Topic | Documentation |
|---|---|
| Building and development | `docs/development.md` |
| BRDF extensions | `docs/brdf-extension.md` |
| Fitting and optimization | `docs/fitting.md` |
| LUT generation | `docs/lut-generation.md` |
| DDS formats and packing | `docs/lut-packing.md` |
| Unreal compatibility | `docs/unreal-compatibility.md` |
| Numerical validation | `docs/validation.md` |
| Benchmarks | `docs/benchmarking.md` |

These filenames are illustrative.

Use actual existing paths.

Do not create broken links.

If a document is missing but its topic is essential, either create a concise document based on verified implementation or link to the most relevant existing document.

Avoid duplicating the same information across multiple documentation files.

---

## 14. References and Attribution

Include appropriate references to the LTC research and source implementation.

At minimum, review:

- Eric Heitz et al., *Real-Time Polygonal-Light Shading with Linearly Transformed Cosines*.
- The official LTC source repository:
  https://github.com/selfshadow/ltc_code
- Relevant Unreal Engine LTC implementation references, when appropriate.

Distinguish this project from the original LTC reference implementation.

Do not imply official endorsement or affiliation with Unreal Engine, Epic Games, or the original LTC authors.

Preserve third-party attribution and license notices.

Do not reproduce large sections of external documentation unnecessarily.

---

## 15. License

Inspect the actual repository license.

Keep the README license section consistent with the `LICENSE` file.

Do not invent or change the project's license.

If the repository has no license, report that fact rather than assuming one.

Check third-party attribution requirements before modifying license documentation.

---

## 16. Documentation Consistency

Review README examples against the actual CLI.

Verify:

- Command names.
- Argument names.
- Default values.
- BRDF identifiers.
- Profile identifiers.
- Output filenames.
- DDS formats.
- Checkpoint syntax.
- Validation syntax.
- Benchmark syntax.
- Supported platforms.

Remove obsolete commands and references to removed functionality.

Ensure terminology is consistent across README and `docs/`.

Use:

- LTC for Linearly Transformed Cosines.
- BRDF for Bidirectional Reflectance Distribution Function.
- DDS for DirectDraw Surface.
- Reference Mode for compatibility-oriented fitting.
- Quality Mode for improved numerical fitting.

Clearly distinguish fitting profiles from export profiles.

---

## 17. Documentation Testing

Perform practical verification of the README.

### CLI Verification

Run the documented commands where feasible.

At minimum:

- Build the application.
- Run `--help`.
- Run `--version`.
- Inspect the available BRDFs.
- Verify the documented generation options.
- Verify the documented export options.
- Verify the documented validation options.

For expensive commands, verify argument syntax using CLI help and use reduced-size test cases where practical.

Do not run a full high-quality 64×64 generation solely to test README examples unless appropriate.

### Link Verification

Check all relative Markdown links.

Ensure that referenced documentation files exist.

### Build Verification

Verify that build instructions match the actual CMake configuration.

Do not claim that an operating system or compiler was tested unless it was actually used.

### Output Verification

Where feasible, generate a small LUT and verify:

- Intermediate file creation.
- DDS export.
- Correct output filenames.
- Successful DDS inspection.

If commands cannot be executed, identify them as unverified in the final report.

---

## 18. Scope Restrictions

This task is documentation-focused.

Do not:

- Change BRDF mathematics.
- Modify the LTC fitting algorithm.
- Change DDS packing conventions.
- Change CLI argument names.
- Change existing output formats.
- Add new BRDF models.
- Add new CLI features solely for documentation.
- Refactor the application architecture.
- Reorganize source code unnecessarily.
- Introduce a documentation website framework.
- Add large generated images or binary assets.
- Add promotional claims without evidence.

Small documentation-related corrections are acceptable when necessary.

If a functional defect is discovered, document it rather than silently changing unrelated application code.

---

## 19. Acceptance Criteria

The task is complete when:

- [ ] README has a clear project introduction.
- [ ] Key implemented features are summarized.
- [ ] Build requirements are accurate.
- [ ] Build instructions reflect the current CMake project.
- [ ] Quick Start is short and executable.
- [ ] GGX LUT generation is clearly documented.
- [ ] DDS export is clearly documented.
- [ ] Supported BRDFs are accurately listed.
- [ ] Reference and Quality modes are explained.
- [ ] Unreal Engine compatibility is described accurately.
- [ ] Numerical validation is introduced with working examples.
- [ ] Advanced topics link to detailed documentation.
- [ ] References and attribution are preserved.
- [ ] License information matches the repository.
- [ ] Obsolete or redundant content is removed.
- [ ] All relative documentation links are valid.
- [ ] CLI command examples are verified where feasible.
- [ ] No unrelated application functionality is changed.
- [ ] README is readable without requiring users to understand the internal implementation.

---

## 20. Implementation Procedure

Follow this order:

1. Inspect the existing README.
2. Inspect CLI help and implemented commands.
3. Review CMake configuration and build instructions.
4. Inspect supported BRDFs and export profiles.
5. Review existing documentation.
6. Identify outdated, duplicated, or misleading content.
7. Plan the revised README structure.
8. Rewrite the introduction and feature overview.
9. Improve build instructions.
10. Create a concise, working Quick Start.
11. Document supported BRDFs and DDS outputs.
12. Summarize validation and advanced workflows.
13. Add a useful documentation index.
14. Preserve references, attribution, and license information.
15. Verify all CLI examples.
16. Verify Markdown links.
17. Review the complete README for clarity and consistency.
18. Run relevant checks.
19. Produce a final change summary.

### Final Report

After implementation, provide:

1. Summary of README improvements.
2. Files modified.
3. Final README section structure.
4. Verified CLI commands.
5. Documentation links checked.
6. Outdated content removed.
7. Any discovered documentation/implementation inconsistencies.
8. Unverified examples or platform-specific limitations.
9. Test or verification results.

**Expected result: A concise, accurate, user-oriented README that allows graphics developers to understand, build, use, and validate the LTC LUT Generator without needing to study the internal numerical fitting implementation first.**
