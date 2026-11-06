# Adding a Custom BRDF

BRDFs are C++ classes that implement `ltc::IBRDF` (`include/ltc/BRDF.h`) and are registered by name in `ltc::BRDFRegistry`. Once a BRDF is registered, `generate`, `fit`, `list-brdfs`, and `benchmark` can use it. There is no plugin or runtime loading mechanism, so adding a BRDF requires rebuilding `ltcgen`.

Use `src/brdf/DisneyDiffuse.cpp` (perceptual-roughness parameter, scalar amplitude) or `src/brdf/GGX.cpp` (microfacet α, Schlick Fresnel) as a starting point.

## 1. Implement `IBRDF`

```cpp
// include/ltc/MyBRDF.h
#pragma once
#include "BRDF.h"

namespace ltc {

class MyBRDF final : public IBRDF {
public:
    std::string_view  name() const override;                         // registry identifier, e.g. "my-brdf"
    BRDFEvaluation    evaluate(const BRDFContext& ctx, const Vec3& light) const override;
    Vec3              sample(const BRDFContext& ctx, const Vec2& u) const override;
    BRDFCapabilities  capabilities() const override;
    double            contextAlpha(double perceptualRoughness) const override;
};

} // namespace ltc
```

Contract:

| Method | Requirement |
|--------|-------------|
| `name()` | Return the same string you register. It is stored in `.ltcbin` files and checked on `--resume`. |
| `evaluate()` | Return the **cosine-weighted** value `f_r(V, L) · max(0, N·L)` and the solid-angle PDF of `sample()` for `L`. Return zero for directions below the horizon or invalid input. |
| `sample()` | Map `u ∈ [0,1)²` to a unit direction. The PDF returned by `evaluate()` must match this distribution; fitting uses MIS. |
| `capabilities()` | Describe the parameter (`parameterization`, `parameterName`), the amplitude model, and Fresnel support. `list-brdfs --details` prints these values. |
| `contextAlpha()` | Map the LUT's perceptual roughness `r ∈ [0,1]` to the value passed as `BRDFContext::alpha`. For example, GGX returns `max(r², 1e-5)` and Disney Diffuse returns `r`. |

Frame: the normal is `+Z` (right-handed), and `V` and `L` both point away from the surface. The view vector always lies in the XZ plane (`V = (sinθ, 0, cosθ)`), and the BRDF is assumed to be isotropic.

All methods must be thread-safe and stateless, because LUT generation calls them concurrently from worker threads.

## 2. Register the BRDF

Add the source file to `ltc_core` in `CMakeLists.txt`, then register a factory in `BRDFRegistry::createDefault()` (`src/brdf/BRDFRegistry.cpp`):

```cpp
r.registerBRDF("my-brdf", [] { return std::make_unique<MyBRDF>(); });
```

Library users can also build their own registry with `BRDFRegistry::registerBRDF()` without changing the default one.

## 3. Test and Validate

1. Add unit tests under `tests/` (see `tests/DisneyDiffuseTests.cpp`). Check energy conservation, that the PDF integrates to 1, and that `evaluate`/`sample` are consistent.
2. Fit a few single points: `ltcgen fit --brdf my-brdf --alpha 0.3 --ndotv 0.7`. Use `--roughness` instead if `parameterization` is `PerceptualRoughness`.
3. Generate a small table and export it with an explicit filename prefix:

   ```bash
   ltcgen generate --brdf my-brdf --width 16 --height 16 --threads auto --output my.ltcbin
   ltcgen export --input my.ltcbin --brdf MyBRDF --output out
   ```

`validate distribution` currently always evaluates GGX (see [validation.md](validation.md#known-issues)). Until that is fixed, it cannot validate a custom BRDF.

The packed texture layout is the same for every BRDF (see [lut-packing.md](lut-packing.md)). How the shader interprets the amplitude channels depends on your BRDF.
