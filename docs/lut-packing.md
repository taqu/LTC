# LUT Files and Texture Packing

This document describes the files `ltcgen` produces: the intermediate `.ltcbin` table and the DDS textures written by `ltcgen export`.

## .ltcbin File

`generate --output`, `--checkpoint`, and `--resume` all use the same little-endian binary format (`src/lut/Checkpoint.cpp`):

| Offset | Size | Field |
|--------|------|-------|
| 0 | 8 | Magic `LTCBIN01` |
| 8 | 4 | Format version |
| 12 | 4 | Width (roughness axis) |
| 16 | 4 | Height (view axis) |
| 20 | 4 | Fitting profile (0 = reference, 1 = quality) |
| 24 | 4 | Sample resolution |
| 28 | 4 | Max iterations |
| 32 | 24 | Simplex step, tolerance, minimum scale (`double`) |
| 56 | 64 | BRDF identifier, null-padded |
| 120 | 8 | Flags (bit 0 = all cells completed) |
| 128 | … | One record per cell, row-major (`a + t * width`) |
| end | 8 | Checksum over header and cells |

The file is written to `<path>.tmp` first and then renamed over the target. Treat the format as internal: it may change between versions, and the version field is checked on load.

## Export Profiles

`ltcgen export --profile` selects the file naming and amplitude layout. Export profiles are independent of the fitting profiles (`reference` / `quality`) used by `generate`.

| Profile | Matrix file | Amplitude file | Amplitude format |
|---------|-------------|----------------|------------------|
| `unreal` (default) | `<BRDF>_LTCMat.dds` | `<BRDF>_LTCAmp.dds` | RG16F (RG32F with `--precision fp32`) |
| `generic` | `<BRDF>_LTCMat.dds` | `<BRDF>_LTCAmp.dds` | RG16F (RG32F with `--precision fp32`) |
| `reference` | `<BRDF>_LTCMat_ref.dds` | `<BRDF>_LTCAmp_ref.dds` | Always RGBA16F |

`<BRDF>` is the `--brdf` value passed to `export` (default `GGX`). It is not read from the `.ltcbin` file, so set it explicitly for non-GGX tables.

The matrix texture is RGBA16F by default, or RGBA32F with `--precision fp32`.

All textures are 2D, one mip level, written with a DX10 extended header. The texture size equals the LUT size.

## Matrix Texture

For each cell, the fitted LTC matrix `M` is inverted and divided by its center element, so `M⁻¹[1][1] = 1`. Four elements are then stored (GLM `[column][row]` indexing, `src/export/LUTPacking.cpp`):

| Channel | Element |
|---------|---------|
| R | `M⁻¹[0][0]` |
| G | `M⁻¹[0][2]` |
| B | `M⁻¹[2][0]` |
| A | `M⁻¹[2][2]` |

The remaining elements of the normalized inverse are 0, except the center element, which is 1. A shader rebuilds the 3×3 inverse matrix from these four values. The resulting transform is defined up to scale, which LTC polygon integration does not depend on.

Make sure your shader's row/column convention matches the table above. Mixing conventions transposes the off-diagonal terms.

## Amplitude Texture

| Channel | `unreal` / `generic` | `reference` |
|---------|----------------------|-------------|
| R | LTC magnitude (BRDF albedo for the cell) | same |
| G | Fresnel coefficient | same |
| B | — | 0 (reserved) |
| A | — | Spherical-cap coefficient `2π(1 - N·V)` |

G holds the BRDF-weighted integral of the Schlick term `(1 - V·H)⁵` (`src/fitting/BRDFIntegration.cpp`). For BRDFs with Schlick Fresnel support (`ggx`, `beckmann`; see `ltcgen list-brdfs --details`), the LTC reference demo shader combines the two channels as `F0 * R + (1 - F0) * G`. The tool does not enforce this formula; confirm it against your runtime.

For `disney-diffuse`, which reports `fresnel: no`, the G channel is still written with a computed value. Shaders should ignore it.

## Texture Coordinates

Texel `(x, y)` holds the fit for grid cell `(a = x, t = y)` (see [lut-generation.md](lut-generation.md#parameterization)). To sample at texel centers:

```text
u = r                  * (W - 1) / W + 0.5 / W     // r = perceptual roughness
v = sqrt(1 - N·V)      * (H - 1) / H + 0.5 / H
```

The roughness value for `ggx` and `beckmann` is perceptual roughness (α = r²). Disney Diffuse uses perceptual roughness directly.

## Unreal Engine Notes

The `unreal` profile targets the GGX LTC texture convention used by Unreal Engine: the file names, the RGBA16F matrix and RG16F amplitude formats, and the normalized four-coefficient inverse-matrix layout.

The repository does not contain Unreal reference textures, and the output has not been verified against a specific Unreal Engine version. To compare against your engine's textures, see [reference/README.md](../reference/README.md) and [validation.md](validation.md#reference-comparison).

Only GGX tables are meaningful replacements for Unreal's GGX LTC textures.
