# LUT Generation

This document covers `ltcgen generate` and `ltcgen fit` in more detail than the [README](../README.md).

## Parameterization

A LUT of `width × height` cells is fitted on this grid (`include/ltc/ParameterGrid.h`):

| Axis | Index | Parameter |
|------|-------|-----------|
| X (roughness) | `a ∈ [0, width-1]` | perceptual roughness `r = a / (width - 1)` |
| Y (view angle) | `t ∈ [0, height-1]` | `cosθ = 1 - (t / (height - 1))²`, with θ clamped to at most 1.57 rad |

The view direction is `V = (sinθ, 0, cosθ)` with the normal along +Z. Cells are stored row-major: `index = a + t * width`.

Each BRDF maps `r` to its own parameter through `IBRDF::contextAlpha`:

| BRDF | Parameter used for fitting |
|------|----------------------------|
| `ggx`, `beckmann` | `α = max(r², 1e-5)` |
| `disney-diffuse` | `r` (perceptual roughness, unchanged) |

Both `width` and `height` must be at least 2.

## Fitting Profiles

| | Reference Mode (`reference`) | Quality Mode (`quality`) |
|---|---|---|
| Optimizer | Nelder–Mead | Nelder–Mead with multi-start restarts |
| Samples | `--samples` (default 32×32) | Starts at `--samples`, adapts up to 128×128 |
| Parameters | Direct matrix scales | Log-space scales |
| Tolerance | 1e-5 | Same as `FittingConfig::tolerance` (1e-5 from the CLI) |
| Cost | Baseline | Several times higher |

Reference Mode is the compatibility-oriented profile. It follows the original ltc_code fitting procedure: MIS error estimation, a warm-started initial guess taken from the neighboring cell, and the reference simplex step and tolerance.

Quality Mode is intended to lower the numerical fitting error. Whether that is visible in rendered lighting depends on the BRDF and the scene. Check it with the commands in [validation.md](validation.md).

Implementation: `src/fitting/LTCFitter.cpp` (Reference) and `src/fitting/QualityFitter.cpp` (Quality).

## Threads

`--threads N` runs N worker threads, and `--threads auto` uses `std::thread::hardware_concurrency()`. The default is a single thread. Use `ltcgen benchmark scaling` to measure speedup on your machine.

## Output and Summary

`--output file.ltcbin` saves the fitted table. Without `--output`, the table is fitted and summarized but **not saved**.

The summary reports:

- **Converged:** the cell met the tolerance.
- **MaxIter:** the iteration limit was reached. The cell still has a usable fit and is exported.
- **Failed:** zero energy or a numerical failure.
- **Error min/max/mean/RMS:** the fitting objective across cells. These values are not normalized. Cells with very low roughness can dominate the maximum and mean, so use [validation.md](validation.md) to judge quality.

`--json` prints the same summary as a JSON object.

## Checkpoint and Resume

```bash
ltcgen generate --threads auto --checkpoint run.ckpt.ltcbin --checkpoint-interval 64 --output ggx.ltcbin
ltcgen generate --threads auto --resume run.ckpt.ltcbin --output ggx.ltcbin
```

- **Current limitation:** the checkpoint file is written only once, after all cells are fitted, regardless of thread count. Periodic saving is not implemented yet: `--checkpoint-interval` is accepted but has no effect, and an interrupted run leaves no checkpoint. `--resume` works with any complete or partial `.ltcbin` file and skips cells already marked as completed.
- The checkpoint uses the `.ltcbin` format (see [lut-packing.md](lut-packing.md#ltcbin-file)).
- A resume must match the checkpoint's BRDF, width, height, `--samples`, and `--max-iterations`. The fitting profile may differ.
- A mismatched checkpoint is rejected, but the current CLI does not catch that error, so the process terminates abnormally without a message.

## Single-Point Fitting

`ltcgen fit` fits one LTC and prints the result. This is useful for debugging a BRDF or comparing against another implementation.

```bash
ltcgen fit --brdf ggx --alpha 0.25 --ndotv 0.8
ltcgen fit --brdf disney-diffuse --roughness 0.5 --ndotv 0.8 --json
```

- `--alpha` is the final microfacet α (it is **not** squared) and is required for `ggx` and `beckmann`.
- `--roughness` is required for `disney-diffuse`.
- `--ndotv` must be in (0, 1].
- The output includes magnitude, the `m11`, `m22`, and `m13` matrix parameters, initial and final error, iteration count, and the Fresnel coefficient where applicable.

**Known limitation:** `fit --profile quality` currently runs the Reference fitter, so both profiles produce identical results. Use `ltcgen benchmark fit --compare-profiles` to compare the two profiles at a single point.
