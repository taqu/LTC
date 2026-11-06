# Validation and Benchmarks

`ltcgen` has three kinds of checks:

1. **Structural:** is the DDS file well formed, and are its values finite? (`inspect`)
2. **Reference compatibility:** does the output match an existing LUT? (`validate reference`)
3. **Approximation quality:** how well does the LTC reproduce the BRDF and its area-light response? (`validate distribution`, `validate lighting`)

A close match to a reference texture shows compatibility with that table, not lighting accuracy. Both tables can share the same approximation error.

> **Known issues in the current build.** Several validation commands do not yet behave as their names suggest. Read [Known Issues](#known-issues) before relying on their results.

## DDS Inspection

```bash
ltcgen inspect output/GGX_LTCMat.dds                  # metadata
ltcgen inspect output/GGX_LTCAmp.dds --stats          # per-channel min/max/mean, NaN/Inf counts
ltcgen inspect output/GGX_LTCMat.dds --pixel 12,24    # one texel
ltcgen inspect output/GGX_LTCMat.dds --json --pixel 0,0 --stats
```

Supported read formats are RG16F, RGBA16F, RG32F, and RGBA32F, with either a legacy FourCC or a DX10 header. See [dds-format.md](dds-format.md).

`ltcgen export` also prints the maximum and mean quantization error introduced by FP16 (or FP32) encoding for both textures.

## Distribution Validation

```bash
ltcgen validate distribution --input ggx.ltcbin --samples 65536 --max-cells 256 --report distribution.json
```

This compares the fitted LTC distribution with the BRDF for up to `--max-cells` evenly spaced cells, using `--samples` directions per cell. For each cell, the JSON report contains L1, L2, maximum absolute, energy, and cubic errors, plus the direction of the largest error.

## Reference Comparison

```bash
ltcgen validate reference --input generated_dir --reference reference_dir --brdf GGX --report reference.json
```

This compares the matrix textures in two directories channel by channel. The report includes MAE, RMSE, maximum absolute and relative error, NaN/Inf counts, the worst texel, and per-region statistics (for example, low roughness at normal view). The command exits non-zero if the textures are incompatible, such as when files are missing or sizes differ.

**Filename note:** this command looks for `<BRDF>_LTC_Matrix.dds` and `<BRDF>_LTC_Amplitude.dds`, but `export` writes `<BRDF>_LTCMat.dds` and `<BRDF>_LTCAmp.dds`. Until the names are unified, copy or rename the files in both directories:

```bash
mkdir cmp
cp output/GGX_LTCMat.dds cmp/GGX_LTC_Matrix.dds
cp output/GGX_LTCAmp.dds cmp/GGX_LTC_Amplitude.dds
```

To obtain Unreal reference textures, see [reference/README.md](../reference/README.md).

## Area-Light (Lighting) Validation

```bash
ltcgen validate lighting --input ggx.ltcbin --samples 65536 --report lighting.json
```

This evaluates eight fixed rectangular-light scenarios: frontal, tilted, grazing, below the horizon, near-normal view, and high and low roughness. Each scenario compares the LTC polygon integral with a numerical ground truth (stratified sampling over the light area). A scenario passes if the relative error is ≤ 10% or the absolute error is ≤ 0.001. The command exits non-zero if any scenario fails.

## Combined Validation

```bash
mkdir reports
ltcgen validate all --input output --source ggx.ltcbin --reference reference_dir --report-dir reports
```

This runs distribution, lighting, and (if `--reference` is given) reference comparison, then writes `reports/validation_report.json` with a pass/fail/skip summary. It exits non-zero if any check fails. The `--report-dir` directory must already exist.

`validate dds --input <dir> --source <file.ltcbin>` writes a report with distribution metrics computed from the source file.

## Known Issues

Observed with version 0.1.0:

| Command | Issue |
|---------|-------|
| `validate reference` | Expects `_LTC_Matrix.dds` / `_LTC_Amplitude.dds` names, which differ from the exporter's `_LTCMat.dds` / `_LTCAmp.dds` (`src/validation/ValidationRunner.cpp`). The CLI command compares only the matrix texture. |
| `validate lighting` | The ground truth is 0 for most scenarios, so they fail with very large relative errors even for a well-fitted GGX table. Do not use the pass/fail result as a quality measure until this is fixed. `validate all` is affected as well. |
| `validate distribution`, `validate dds` | Always evaluate the GGX BRDF, regardless of the BRDF stored in the `.ltcbin`. Results for Beckmann or Disney Diffuse tables are not meaningful. |
| `validate dds` | `--input` is not read. The command does not check the DDS files, and it succeeds even if the directory does not exist. |
| `validate all` | Fails with `cannot open report file` if `--report-dir` does not exist. |
| `--profile` on `validate` | Only recorded as a label in the report. It does not change the checks performed. |

## Benchmarks

```bash
ltcgen benchmark fit --brdf ggx --alpha 0.25 --ndotv 0.8 --compare-profiles --output fit.json
ltcgen benchmark generate --brdf ggx --width 64 --height 64 --threads 8 --profile reference --output gen.json
ltcgen benchmark scaling --brdf ggx --width 32 --height 32 --threads 1,2,4,8 --output scaling.json
ltcgen benchmark suite --brdfs ggx,beckmann,disney-diffuse --width 32 --height 32 --output bench_dir
```

| Command | Measures |
|---------|----------|
| `benchmark fit` | Iterations, evaluations, final objective, and time for one fit. Use `--compare-profiles` to run both Reference and Quality Mode. |
| `benchmark generate` | Total and per-cell time, converged and max-iteration cells, mean error for a full LUT |
| `benchmark scaling` | Generation time and speedup for each thread count |
| `benchmark suite` | Reference and Quality generation for each listed BRDF; writes `suite_report.json` into `--output` |

`benchmark suite` runs single-threaded and can take a long time at large resolutions. Benchmark reports record the build configuration as `Release`, regardless of how the tool was built.
