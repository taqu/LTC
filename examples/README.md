# Examples

Example `ltcgen` workflows. All commands were checked against version 0.1.0. Each command fits on one line, so it works the same in PowerShell and in Unix shells.

## GGX LUT for Unreal-style DDS output

```bash
ltcgen generate --brdf ggx --width 64 --height 64 --threads auto --output ggx.ltcbin
ltcgen export --input ggx.ltcbin --profile unreal --output output
ltcgen inspect output/GGX_LTCMat.dds
ltcgen inspect output/GGX_LTCAmp.dds --stats
```

## FP32 output

```bash
ltcgen export --input ggx.ltcbin --precision fp32 --output output_fp32
```

## Quality Mode

```bash
ltcgen generate --brdf ggx --profile quality --threads auto --output ggx_quality.ltcbin
ltcgen benchmark fit --alpha 0.25 --ndotv 0.8 --compare-profiles
```

## Other BRDFs

```bash
ltcgen list-brdfs --details
ltcgen generate --brdf disney-diffuse --threads auto --output diffuse.ltcbin
ltcgen export --input diffuse.ltcbin --brdf DisneyDiffuse --output output
```

## Fast smoke test

```bash
ltcgen generate --width 16 --height 16 --threads auto --output small.ltcbin
ltcgen export --input small.ltcbin --output small
ltcgen validate distribution --input small.ltcbin --samples 4096 --max-cells 16 --report small_distribution.json
```

## DDS inspection

```bash
ltcgen inspect texture.dds --json
ltcgen inspect texture.dds --pixel 12,24
ltcgen inspect texture.dds --json --pixel 0,0 --stats
```

For more detail, see the [README](../README.md) and [docs/](../docs/).
