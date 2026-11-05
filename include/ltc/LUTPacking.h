#pragma once
#include "LUT.h"
#include "TextureData.h"

namespace ltc {

enum class ExportProfile { Unreal, Reference, Generic };

struct PackedLTCMatrix {
    double r, g, b, a; // channel values before quantization
};

// Pack LTC inverse matrix: normalize by center diagonal, extract 4 components
// GLM column-major: invM[col][row]
// r=invM[0][0], g=invM[0][2], b=invM[2][0], a=invM[2][2] after /= invM[1][1]
// Throws on degenerate matrix or non-finite result
PackedLTCMatrix packLTCMatrix(const Mat3& matrix);

struct PackedAmplitude {
    double magnitude;
    double fresnelCoefficient;
    double sphericalCap; // Reference profile only; 0 for Unreal
};

// Compute spherical cap integration coefficient: 2*pi*(1 - cos(theta_v))
// where theta_v is the view angle (ndotv = cos(theta_v))
// Clamp ndotv to [0, 1], handle edge cases
double computeSphericalCapCoefficient(double ndotv);

// Build matrix TextureData from LUTData
// x=roughnessIndex, y=viewIndex, pixelIndex = y*width + x
// For Reference profile: RGBA16F with 4 LTC matrix coefficients
// For Unreal profile: RGBA16F with 4 LTC matrix coefficients (same layout)
TextureData buildMatrixTexture(const LUTData& lut, ExportProfile profile, TextureFormat fmt);

// Build amplitude TextureData from LUTData
// Unreal: RG16F, R=magnitude, G=fresnelCoefficient
// Reference: RGBA16F, R=magnitude, G=fresnelCoefficient, B=0, A=sphericalCap
TextureData buildAmplitudeTexture(const LUTData& lut, ExportProfile profile, TextureFormat fmt);

} // namespace ltc
