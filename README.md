# LTC LUT Generator

LTC LUT Generator (`ltcgen`) is a standalone C++20 command-line tool for fitting Linearly Transformed Cosines (LTC) to BRDF distributions and generating DDS lookup textures for real-time area-light rendering.

Fitting runs entirely on the CPU and can use multiple worker threads. The tool fits several BRDF models, writes the fitted table to an intermediate `.ltcbin` file, and exports it as floating-point DDS textures for use in shaders. Validation and benchmark commands are included for checking the results. The default export layout follows the GGX LTC texture conventions used by Unreal Engine (`GGX_LTCMat.dds` / `GGX_LTCAmp.dds`). This is a standalone tool, not an Unreal Engine plugin.

## Features

- CPU LTC fitting (Nelder–Mead) based on the reference [ltc_code](https://github.com/selfshadow/ltc_code) approach
- Built-in BRDFs: GGX, Beckmann, Disney (Burley) Diffuse
- Custom BRDFs through a C++ interface and registry
- Two fitting profiles: **Reference Mode** and **Quality Mode**
- Multithreaded LUT generation
- Checkpoint and resume (`.ltcbin` state; periodic saving is not implemented yet)
- DDS export (RGBA16F/RG16F, or FP32) with three export profiles
- DDS inspection: metadata, single pixels, per-channel statistics, JSON output
- Numerical validation: LTC distribution error, reference texture comparison, area-light comparison
- Fitting and generation benchmarks with JSON reports

## Requirements

| Item | Requirement |
|------|-------------|
| CMake | 3.21 or newer |
| Compiler | C++20: MSVC 19.29+ (Visual Studio 2022), GCC 11+, or Clang 13+ |
| Git + network access | Needed on the first configure; CMake `FetchContent` downloads CLI11, nlohmann/json, GLM, and GoogleTest |

The build is developed and tested on Windows with Visual Studio 2022. Linux and macOS presets and compiler flags exist, but those platforms have not been verified for this release.

## Quick Start

Build the tool (Windows, Visual Studio 2022):

```powershell
cmake --preset windows-msvc-release
cmake --build build/msvc-release --config Release
```

On Linux or macOS:

```bash
cmake --preset linux-release
cmake --build build/linux-release
```

The executable is written to:

| Preset | Executable |
|--------|------------|
| `windows-msvc-release` | `build/msvc-release/Release/ltcgen.exe` |
| `windows-msvc-debug` | `build/msvc-debug/Debug/ltcgen.exe` |
| `linux-release` | `build/linux-release/ltcgen` |
| `linux-debug` | `build/linux-debug/ltcgen` |

Visual Studio is a multi-configuration generator, so always pass `--config` when building; without it, MSBuild builds `Debug` regardless of the preset name.

Generate a 64×64 GGX LUT, export it as DDS, and inspect the result:

```bash
ltcgen generate --brdf ggx --threads auto --output ggx.ltcbin
ltcgen export --input ggx.ltcbin --output output
ltcgen inspect output/GGX_LTCMat.dds
```

This produces:

```text
output/
├── GGX_LTCMat.dds   (64×64, RGBA16F)
└── GGX_LTCAmp.dds   (64×64, RG16F)
```

A 64×64 Reference Mode run takes a few seconds on a modern multi-core CPU. Generation and export are separate steps; there is no single command that does both.

## Generating LTC LUTs

`ltcgen generate` fits one LTC per LUT cell and saves the table as a `.ltcbin` file.

```bash
ltcgen generate --brdf ggx --width 64 --height 64 --profile reference --threads auto --output ggx.ltcbin
```

| Option | Default | Meaning |
|--------|---------|---------|
| `--brdf` | `ggx` | BRDF identifier (see `ltcgen list-brdfs`) |
| `--width` | `64` | Roughness axis resolution (≥ 2) |
| `--height` | `64` | View-angle axis resolution (≥ 2) |
| `--profile` | `reference` | Fitting profile: `reference` or `quality` |
| `--threads` | `1` | Worker threads, or `auto` for all hardware threads |
| `--output` | *(none)* | Output `.ltcbin` file. **If omitted, the result is not saved.** |
| `--samples` | `32` | N×N samples per error evaluation |
| `--max-iterations` | `100` | Nelder–Mead iteration limit |
| `--json` | off | Print the summary as JSON |

After fitting, the command prints a summary (converged, max-iteration, and failed cells, plus fitting error statistics).

### Reference Mode and Quality Mode

- **Reference Mode** (`--profile reference`) follows the fitting conventions of the original LTC reference implementation: 32×32 samples, MIS-based error, and Nelder–Mead with the reference step size and tolerance. Use this mode when you want results comparable to established LTC tables.
- **Quality Mode** (`--profile quality`) adds adaptive sampling, multi-start optimization, and log-space scale parameters to reduce the fitting error. It costs several times more CPU time than Reference Mode (about 5–11× in `ltcgen benchmark suite` on small tables). A lower fitting error does not guarantee visibly better lighting, so validate the results for your use case.

`--profile` is a *fitting* profile. It is unrelated to the *export* profile used by `ltcgen export`.

### Checkpoint and Resume

`generate` can write a checkpoint and later resume from one:

```bash
ltcgen generate --profile quality --threads auto --checkpoint ggx.ckpt.ltcbin --checkpoint-interval 64 --output ggx.ltcbin
ltcgen generate --profile quality --threads auto --resume ggx.ckpt.ltcbin --output ggx.ltcbin
```

A checkpoint stores fitted LUT state in the same `.ltcbin` format. It is not a texture. **In the current build, the checkpoint is written only when generation finishes.** `--checkpoint-interval` has no effect yet, so an interrupted run cannot be resumed. When resuming, pass the same `--brdf`, `--width`, `--height`, `--samples`, and `--max-iterations` values. A mismatched checkpoint is rejected, but the current build exits abnormally without an error message.

See [docs/lut-generation.md](docs/lut-generation.md) for parameterization, threading, checkpoints, and the single-point `fit` command.

## Supported BRDFs

| Identifier | BRDF | Roughness parameter |
|------------|------|---------------------|
| `ggx` | Isotropic GGX (Trowbridge–Reitz) microfacet reflection | Microfacet α = roughness² |
| `beckmann` | Isotropic Beckmann microfacet reflection | Microfacet α = roughness² |
| `disney-diffuse` | Disney (Burley) diffuse reflection | Perceptual roughness, used directly |

```bash
ltcgen list-brdfs --details
```

The LUT roughness axis always spans perceptual roughness 0–1, but each BRDF maps it to its own parameter. The same roughness value does not have the same physical meaning across models, so a Beckmann or Disney Diffuse table is not interchangeable with a GGX table.

Generating a non-GGX LUT: pass `--brdf` to both commands. The export `--brdf` value only sets the filename prefix and defaults to `GGX`.

```bash
ltcgen generate --brdf beckmann --threads auto --output beckmann.ltcbin
ltcgen export --input beckmann.ltcbin --brdf Beckmann --output output
```

To add your own BRDF, see [docs/brdf-extension.md](docs/brdf-extension.md).

## DDS Output

`ltcgen export` converts a `.ltcbin` file into two DDS textures (DX10 header, mip level 0 only):

| Output (default `unreal` profile) | Format | Contents |
|-----------------------------------|--------|----------|
| `GGX_LTCMat.dds` | RGBA16F | Four coefficients of the inverse LTC matrix, normalized so the center element is 1 |
| `GGX_LTCAmp.dds` | RG16F | R = LTC magnitude (BRDF albedo), G = Fresnel coefficient |

| Option | Default | Values |
|--------|---------|--------|
| `--input` | *(required)* | `.ltcbin` file from `generate` |
| `--output` | `.` | Output directory (created if missing) |
| `--profile` | `unreal` | `unreal`, `reference`, or `generic` |
| `--brdf` | `GGX` | Filename prefix |
| `--precision` | `fp16` | `fp16` or `fp32` (`fp32` writes RGBA32F / RG32F) |

The `reference` export profile writes `<BRDF>_LTCMat_ref.dds` and an RGBA16F `<BRDF>_LTCAmp_ref.dds` with an additional spherical-cap coefficient in the A channel. The `generic` profile uses the same names and layout as `unreal`.

The textures contain floating-point coefficients for shader use. They are only correct when your shader uses the same texture coordinate mapping (roughness on X, `sqrt(1 - N·V)` on Y) and rebuilds the matrix with the same packing convention. The exact layout is documented in [docs/lut-packing.md](docs/lut-packing.md).

### Unreal Engine Compatibility

- **Naming:** the `unreal` profile uses Unreal-style filenames.
- **Packing:** the matrix and amplitude layouts are designed to match the GGX LTC convention used by Unreal Engine.
- **Verification:** compatibility has not been verified against a specific Unreal Engine version in this repository. No Unreal reference textures are included (see [reference/README.md](reference/README.md)).

Only GGX tables are candidates for replacing Unreal's GGX LTC textures. Beckmann and Disney Diffuse tables use the same file layout but encode different BRDFs.

## Validation

Inspect the exported textures:

```bash
ltcgen inspect output/GGX_LTCMat.dds
ltcgen inspect output/GGX_LTCAmp.dds --stats
ltcgen inspect output/GGX_LTCMat.dds --pixel 12,24 --json
```

Measure how closely the fitted LTC distributions match the BRDF, and write a JSON report:

```bash
ltcgen validate distribution --input ggx.ltcbin --report distribution.json
```

Other validation commands:

| Command | Checks |
|---------|--------|
| `validate distribution` | LTC vs. BRDF distribution error (L1, L2, max, energy) for a subset of cells |
| `validate reference` | Generated DDS vs. reference DDS (coefficient differences) |
| `validate lighting` | LTC area-light integrals vs. numerical ground truth |
| `validate dds` | Distribution metrics computed from the source `.ltcbin` |
| `validate all` | All of the above; writes `validation_report.json` |

`ltcgen export` also reports the FP16 (or FP32) quantization error of the written textures.

Matching a reference texture shows compatibility with that table, not lighting accuracy. Distribution and area-light validation measure the approximation quality directly. Several validation commands currently have known issues, including a filename mismatch in `validate reference` and an all-zero ground truth in `validate lighting`. Read [docs/validation.md](docs/validation.md) before relying on their results.

## Advanced Usage

- **Single-point fitting:** `ltcgen fit --alpha 0.25 --ndotv 0.8` fits one LTC and prints the matrix coefficients. Useful for debugging. See [docs/lut-generation.md](docs/lut-generation.md).
- **Benchmarks:** `ltcgen benchmark fit|generate|suite|scaling` measures fitting cost, Reference vs. Quality error, and thread scaling. See [docs/validation.md](docs/validation.md#benchmarks).
- **Custom BRDFs:** see [docs/brdf-extension.md](docs/brdf-extension.md).
- **Intermediate `.ltcbin` format and texture packing:** see [docs/lut-packing.md](docs/lut-packing.md).
- **LUT refinement:** available as a library API (`include/ltc/LUTRefinement.h`); not exposed through the CLI.

## Documentation

| Topic | Document |
|-------|----------|
| Building, testing, and development | [docs/development.md](docs/development.md) |
| Code structure | [docs/architecture.md](docs/architecture.md) |
| LUT generation, fitting profiles, checkpoints | [docs/lut-generation.md](docs/lut-generation.md) |
| `.ltcbin`, DDS export, texture packing, Unreal notes | [docs/lut-packing.md](docs/lut-packing.md) |
| Validation and benchmarks | [docs/validation.md](docs/validation.md) |
| Custom BRDFs | [docs/brdf-extension.md](docs/brdf-extension.md) |
| DDS file format (reader) | [docs/dds-format.md](docs/dds-format.md) |
| Reference textures | [reference/README.md](reference/README.md) |

## References

- Eric Heitz, Jonathan Dupuy, Stephen Hill, David Neubelt. *Real-Time Polygonal-Light Shading with Linearly Transformed Cosines.* ACM Transactions on Graphics (SIGGRAPH 2016).
- Original LTC source code and fitting tools: <https://github.com/selfshadow/ltc_code>
- Brent Burley. *Physically-Based Shading at Disney.* SIGGRAPH 2012 Course: Practical Physically Based Shading in Film and Game Production.

This project is an independent implementation. It is not affiliated with or endorsed by the original LTC authors, Epic Games, or Unreal Engine. Unreal Engine is a trademark of Epic Games, Inc.

### Third-Party Libraries

Fetched at configure time; not redistributed in this repository.

| Library | Version | License |
|---------|---------|---------|
| [CLI11](https://github.com/CLIUtils/CLI11) | v2.4.2 | BSD-3-Clause |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.11.3 | MIT |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | MIT (or Happy Bunny License) |
| [GoogleTest](https://github.com/google/googletest) | v1.15.2 | BSD-3-Clause (tests only) |

## License

MIT. See [LICENSE](LICENSE). Copyright (c) 2026 taqu.
