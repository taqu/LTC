# Reference Files

This directory is intended to hold reference DDS textures from Unreal Engine.

## Expected Files

- `unreal/GGX_LTCMat.dds` — LTC matrix coefficients texture
- `unreal/GGX_LTCAmp.dds` — LTC amplitude texture

## Obtaining Reference Files

These files are not included in this repository. To obtain them:

1. Install Unreal Engine via the Epic Games Launcher.
2. Locate the engine content directory.
3. Search for `GGX_LTCMat.dds` and `GGX_LTCAmp.dds`.

Verify the licensing terms before redistributing these files.

## Inspecting Reference Files

Once placed in `reference/unreal/`, inspect them with:

```
ltcgen inspect reference/unreal/GGX_LTCMat.dds
ltcgen inspect reference/unreal/GGX_LTCAmp.dds --stats
```

## Comparing Against Generated Output

`ltcgen validate reference` currently expects the files to be named `GGX_LTC_Matrix.dds` and `GGX_LTC_Amplitude.dds` in both directories. This differs from the exporter's `GGX_LTCMat.dds` and `GGX_LTCAmp.dds`, so copy or rename the files before comparing. See [docs/validation.md](../docs/validation.md#reference-comparison).
