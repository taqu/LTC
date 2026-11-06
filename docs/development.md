# Development Guide

## Prerequisites

| Tool | Minimum version | Notes |
|------|-----------------|-------|
| CMake | 3.21 | Required for `FetchContent` and presets |
| C++ compiler | C++20 | MSVC 19.29+ / GCC 11+ / Clang 13+ |
| Git | Any recent version | Used by `FetchContent` to download dependencies |

On Windows, Visual Studio 2022 (any edition) is the recommended and tested toolchain. The Linux presets have not been verified for this release.

## Repository Layout

```text
├── cmake/               # CompilerOptions.cmake (ltc_set_compiler_options)
├── docs/                # User and developer documentation
│   └── work/            # Planning documents (do not edit)
├── examples/            # Usage examples
├── include/ltc/         # Public headers of ltc_core
├── reference/           # Placeholder for external reference DDS textures (not committed)
├── src/
│   ├── benchmark/       # benchmark subcommands
│   ├── brdf/            # GGX, Beckmann, Disney Diffuse, BRDFRegistry
│   ├── cli/             # ltcgen entry point (main.cpp)
│   ├── core/            # Version, math, sampling helpers
│   ├── dds/             # DDS reader/writer, FP16 conversion
│   ├── export/          # LUT packing and DDS export
│   ├── fitting/         # LTC model, error function, Nelder–Mead, Quality fitter
│   ├── lut/             # Parameter grid, generator, scheduler, checkpoint, refinement
│   └── validation/      # Distribution, reference, and area-light validation
└── tests/               # GoogleTest suite (ltc_tests)
```

See [architecture.md](architecture.md) for component responsibilities.

## Building

### Presets

| Preset | Generator | Build directory |
|--------|-----------|-----------------|
| `windows-msvc-debug` | Visual Studio 17 2022 | `build/msvc-debug` |
| `windows-msvc-release` | Visual Studio 17 2022 | `build/msvc-release` |
| `linux-debug` | Default generator | `build/linux-debug` |
| `linux-release` | Default generator | `build/linux-release` |

All presets set `LTC_BUILD_TESTS=ON`.

### Windows (Visual Studio 2022)

```powershell
cmake --preset windows-msvc-release
cmake --build build/msvc-release --config Release
ctest --test-dir build/msvc-release -C Release --output-on-failure
```

Visual Studio is a multi-configuration generator. The preset's `CMAKE_BUILD_TYPE` is ignored, so pass `--config` to `cmake --build` and `-C` to `ctest`. Without them, a `Debug` build is produced. Outputs are written to a per-configuration subdirectory:

- `build/msvc-release/Release/ltcgen.exe`
- `build/msvc-release/tests/Release/ltc_tests.exe`

### Linux / macOS

```bash
cmake --preset linux-release
cmake --build build/linux-release
ctest --test-dir build/linux-release --output-on-failure
```

These presets use a single-configuration generator, so the outputs are `build/linux-release/ltcgen` and `build/linux-release/tests/ltc_tests`.

### Without presets

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLTC_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Set `-DLTC_BUILD_TESTS=OFF` to skip downloading GoogleTest and building the test suite.

## Dependencies

All dependencies are fetched by `FetchContent` on the first configure:

| Library | Version | Use |
|---------|---------|-----|
| [CLI11](https://github.com/CLIUtils/CLI11) | v2.4.2 | Command-line parsing (`ltcgen` only) |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.11.3 | JSON output and reports |
| [GLM](https://github.com/g-truc/glm) | 1.0.1 | Vector and matrix math |
| [GoogleTest](https://github.com/google/googletest) | v1.15.2 | Tests (only when `LTC_BUILD_TESTS=ON`) |

An internet connection is required on the first configure. After that, the sources are cached in `<build>/_deps`.

## Compiler Options

`ltc_set_compiler_options()` (in `cmake/CompilerOptions.cmake`) applies:

- **MSVC:** `/W4 /WX /permissive- /Zc:__cplusplus /utf-8`, `/O2` in Release, plus `_CRT_SECURE_NO_WARNINGS`, `NOMINMAX`, and `WIN32_LEAN_AND_MEAN`
- **GCC / Clang:** `-Wall -Wextra -Wpedantic -Werror`, `-O2` in Release, `-g` in Debug

Warnings are treated as errors.

## Running Tests

```bash
# All tests
ctest --test-dir build/msvc-release -C Release --output-on-failure

# Filter by test name
build/msvc-release/tests/Release/ltc_tests.exe --gtest_filter="FP16Test.*"
```

`CLITests` runs the `ltcgen` executable as a subprocess. Its path is baked in at compile time through the `LTCGEN_EXECUTABLE` macro (`tests/CMakeLists.txt`).

Some tests run small LUT generations. Use a Release build for faster test runs.

## Adding Features

1. Add public types and interfaces to `include/ltc/`, and the implementation under the matching `src/` subdirectory.
2. Add new source files to the `ltc_core` target in `CMakeLists.txt`.
3. Expose CLI options in `src/cli/main.cpp`.
4. Add unit tests in `tests/` and register them in `tests/CMakeLists.txt`. Add CLI tests to `tests/CLITests.cpp`.
5. Update the README and the relevant document in `docs/`.

To add a BRDF, see [brdf-extension.md](brdf-extension.md).

## Code Style

- C++20 without compiler extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- Use `std::memcpy` for type punning, never `reinterpret_cast` on file data.
- No global or static mutable state. BRDFs and fitters must be safe to call from multiple threads.
- Use `std::filesystem::path` for paths and binary-mode streams for file I/O.
