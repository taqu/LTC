#pragma once
#include <string>

namespace ltc {

enum class AmplitudeModel { Scalar, SchlickFresnel };
enum class BRDFParameterization { MicrofacetAlpha, PerceptualRoughness };

struct BRDFCapabilities {
    bool isotropic                  = true;
    bool reciprocal                 = true;
    bool supportsImportanceSampling = true;
    bool supportsSchlickFresnel     = false;
    AmplitudeModel amplitudeModel   = AmplitudeModel::Scalar;
    BRDFParameterization parameterization = BRDFParameterization::MicrofacetAlpha;
    double minRoughness = 0.0;
    double maxRoughness = 1.0;
    std::string parameterName = "alpha";  // display name for the roughness param
};

} // namespace ltc
