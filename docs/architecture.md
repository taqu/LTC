# Architecture

## Overview

The LTC LUT Generator is structured as a reusable library (`ltc_core`) and a CLI tool (`ltcgen`).

```
LTCGenerator/
├── include/ltc/        # Public headers
│   ├── Version.h       # Version constants and string
│   ├── DDS.h           # DDS types: DDSFormat, DDSMetadata, Pixel, format helpers
│   └── DDSReader.h     # DDSReader, DDSError, ChannelStats, ImageStats
├── src/
│   ├── core/
│   │   └── Version.cpp # Version::string() implementation
│   ├── dds/
│   │   └── DDSReader.cpp  # DDS parsing and pixel decoding
│   └── cli/
│       └── main.cpp    # CLI entry point (CLI11 + nlohmann/json)
├── tests/
│   ├── DDSReaderTests.cpp  # Unit tests for DDSReader and fp16 conversion
│   └── CLITests.cpp        # Integration tests via subprocess
└── cmake/
    └── CompilerOptions.cmake  # ltc_set_compiler_options() function
```

## Component Responsibilities

### ltc_core (static library)

- **Version**: single source of truth for the version string.
- **DDSReader**: reads DDS files in binary mode, parses headers (legacy FourCC and DX10 extended), decodes pixels, and computes per-channel statistics.  Throws `DDSError` for all error conditions.
- **DDS types** (DDS.h): plain data types (`DDSMetadata`, `Pixel`, `DDSFormat`) and three free functions (`formatToString`, `formatChannelCount`, `formatBytesPerPixel`).

### ltcgen (executable)

Thin CLI wrapper built with CLI11.  Subcommands:

| Subcommand | Function |
|------------|----------|
| `inspect`  | Print DDS metadata, optionally a pixel, optionally per-channel stats |

Output is either human-readable text (default) or JSON (`--json`).  All errors go to stderr; the process exits non-zero on any error.

### Tests

- `DDSReaderTests.cpp` — pure unit tests that create synthetic DDS files in temp directories and validate parsing, pixel reads, statistics, error handling, and fp16 conversion.
- `CLITests.cpp` — integration tests that invoke the `ltcgen` binary as a subprocess and check exit codes and output strings.

## Design Principles

- **No global state**: all functions are static methods or free functions.
- **Fail fast**: every file read is bounds-checked before it happens; overflow is detected before multiplying dimensions.
- **Safe type punning**: `std::memcpy` is used throughout instead of `reinterpret_cast` on raw file bytes.
- **Cross-platform**: `std::filesystem` and `std::ifstream` are used exclusively; no platform-specific I/O calls appear in the library code.
