# Development Guide

## Prerequisites

| Tool | Minimum version | Notes |
|------|----------------|-------|
| CMake | 3.21 | Required for `FetchContent` and presets |
| C++ compiler | C++20 support | MSVC 19.29+ / GCC 11+ / Clang 13+ |
| Git | Any recent | Needed by FetchContent for dependencies |

On Windows, Visual Studio 2022 (any edition) is the recommended toolchain.

## Repository Layout

```
LTCGenerator/
├── cmake/                  # CMake helper modules
├── docs/                   # Documentation
│   └── work/               # Planning documents (do not edit)
├── examples/               # Usage examples
├── include/ltc/            # Public headers (installed with the library)
├── reference/              # Placeholder for reference DDS textures
├── src/
│   ├── cli/                # ltcgen executable source
│   ├── core/               # Shared utilities (Version)
│   └── dds/                # DDS reader implementation
└── tests/                  # GTest unit and integration tests
```

## Building

### Windows (Visual Studio 2022)

```powershell
# Configure (downloads CLI11, nlohmann/json, GoogleTest via FetchContent)
cmake --preset windows-msvc-debug

# Build
cmake --build build/msvc-debug

# Run tests
ctest --test-dir build/msvc-debug --output-on-failure
```

For a release build, replace `windows-msvc-debug` with `windows-msvc-release`.

### Linux / macOS

```bash
cmake --preset linux-debug
cmake --build build/linux-debug
ctest --test-dir build/linux-debug --output-on-failure
```

### Manual configure (no presets)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DLTC_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Dependencies

All dependencies are fetched automatically by CMake's `FetchContent` on first configure:

| Library | Version | Use |
|---------|---------|-----|
| [CLI11](https://github.com/CLIUtils/CLI11) | v2.4.2 | Command-line argument parsing |
| [nlohmann/json](https://github.com/nlohmann/json) | v3.11.3 | JSON serialization |
| [GoogleTest](https://github.com/google/googletest) | v1.15.2 | Unit and integration tests |

An internet connection is required on first build. After the initial fetch, CMake caches the sources in the build directory.

## Compiler Options

The `ltc_set_compiler_options()` function (defined in `cmake/CompilerOptions.cmake`) applies:

**MSVC:**
- `/W4 /WX` — high warning level, warnings as errors
- `/permissive-` — strict standard conformance
- `/Zc:__cplusplus` — correct `__cplusplus` macro value
- `/utf-8` — source and execution charset UTF-8
- `_CRT_SECURE_NO_WARNINGS`, `NOMINMAX`, `WIN32_LEAN_AND_MEAN` — common Windows suppression macros

**GCC / Clang:**
- `-Wall -Wextra -Wpedantic -Werror` — high warning level, warnings as errors

## Running Tests

After building, tests can be run individually:

```bash
# All tests
ctest --test-dir build/msvc-debug --output-on-failure

# Specific test binary
build/msvc-debug/tests/Debug/ltc_tests.exe --gtest_filter="FP16Test.*"
```

The `CLITests` suite invokes the `ltcgen` executable as a subprocess.  The path is baked in at compile time via the `LTCGEN_EXECUTABLE` macro defined in `tests/CMakeLists.txt`.

## Adding New Features

1. Add public types/interfaces to `include/ltc/`.
2. Implement in `src/core/` or `src/dds/` as appropriate.
3. Expose via the CLI in `src/cli/main.cpp`.
4. Add unit tests to `tests/DDSReaderTests.cpp` and CLI integration tests to `tests/CLITests.cpp`.
5. Update `docs/architecture.md` and `docs/dds-format.md` as needed.

## Code Style

- C++20, no extensions (`CMAKE_CXX_EXTENSIONS OFF`).
- `std::memcpy` for type punning; never `reinterpret_cast` on file data.
- All errors reported as `ltc::DDSError` exceptions with descriptive messages.
- No global or static mutable state.
- `std::filesystem::path` for all file paths.
- `std::ifstream` with `std::ios::binary` for all file I/O.
