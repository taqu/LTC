# LTC LUT Generator

A C++20 tool for generating and inspecting Linearly Transformed Cosines (LTC) look-up textures used in real-time area light rendering.

## Overview

LTC LUTs encode the parameters of a linearly transformed cosine distribution that approximates the BRDF response for area lights. This tool provides:

- **DDS inspection** — read metadata, individual pixels, and per-channel statistics from DDS texture files (Phase 0, complete).
- **LTC fitting** — fit LTC distributions to a measured BRDF and write the result as DDS textures (planned, future phases).

## Platforms

| Platform | Compiler | Status |
|----------|----------|--------|
| Windows 10/11 | MSVC 2022 (19.29+) | Supported |
| Linux | GCC 11+ / Clang 13+ | Supported |
| macOS | Apple Clang 14+ | Should work (untested) |

## Build Requirements

- CMake 3.21 or newer
- A C++20-capable compiler (see table above)
- Git (for FetchContent dependency downloads)
- Internet access on first build (dependencies fetched automatically)

## Building

### Windows — Visual Studio 2022

```powershell
cmake --preset windows-msvc-debug
cmake --build build/msvc-debug
ctest --test-dir build/msvc-debug --output-on-failure
```

### Linux / macOS

```bash
cmake --preset linux-debug
cmake --build build/linux-debug
ctest --test-dir build/linux-debug --output-on-failure
```

### Manual (no presets)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DLTC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## CLI Usage

### Inspect DDS metadata

```
ltcgen inspect texture.dds
```

Example output:

```
File:          texture.dds
Dimensions:    64x64
Mip levels:    1
Array size:    1
Format:        RGBA16F
Channels:      4
Bytes/pixel:   8
Data offset:   148
Data size:     32768
```

### Inspect as JSON

```
ltcgen inspect texture.dds --json
```

### Read a specific pixel

```
ltcgen inspect texture.dds --pixel 12,24
```

### Per-channel statistics

```
ltcgen inspect texture.dds --stats
```

### Combined JSON output with pixel and stats

```
ltcgen inspect texture.dds --json --pixel 0,0 --stats
```

### Version

```
ltcgen --version
```

## Dependencies

| Library | Version | License |
|---------|---------|---------|
| [CLI11](https://github.com/CLIUtils/CLI11) | v2.4.2 | BSD-3-Clause |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.11.3 | MIT |
| [GoogleTest](https://github.com/google/googletest) | v1.15.2 | BSD-3-Clause |

All dependencies are downloaded automatically by CMake on first configure.

## Supported DDS Formats

| Format | Description |
|--------|-------------|
| RG16F | 2-channel 16-bit float |
| RGBA16F | 4-channel 16-bit float |
| RG32F | 2-channel 32-bit float |
| RGBA32F | 4-channel 32-bit float |

Both the legacy FourCC path and the DX10 extended header path are supported.

## Current Limitations

- No BRDF fitting or LTC fitting is implemented yet (planned for Phase 1+).
- Only 2D non-array textures are supported.
- Only the floating-point formats listed above are supported.
- Only mip level 0 is accessible via `readPixel` and `computeStats`.

## Roadmap

| Phase | Description |
|-------|-------------|
| 0 (complete) | Project infrastructure, DDS reader, CLI inspection |
| 1 | GGX BRDF sampling and tabulation |
| 2 | LTC fitting algorithm |
| 3 | DDS LUT output writer |
| 4 | Validation against Unreal Engine reference textures |

## License

MIT — see [LICENSE](LICENSE).  
Copyright (c) 2026 taqu.
